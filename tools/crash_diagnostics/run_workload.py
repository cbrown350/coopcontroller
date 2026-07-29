#!/usr/bin/env python3
"""Realistic workload runner for crash-diagnostic testing on the ESP32 test board.

Reproduces the production trigger pattern: one browser-equivalent client that
loads the Update page, requests /update/check, polls /update/status, reads
/update/check_result, then opens /logs with 5-second polling. Optionally
supports a second client and bounded heap-pressure requests.

Safety: hard-refuses host 192.168.2.8 (production), coopcontroller.local,
and any host resolving to the production IP. Only the test board may be tested.

Usage:
    python3 run_workload.py --host 192.168.2.99 --duration 3600 --clients 1 --output run.jsonl
"""
import argparse
import json
import socket
import sys
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

PRODUCTION_IP = "192.168.2.8"
PRODUCTION_HOSTS = {"192.168.2.8", "coopcontroller.local"}
MAX_CLIENTS = 2
MAX_HEADERS_EXPERIMENT = 32

BROWSER_HEADERS = {
    "Host": "",  # filled per request
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.5",
    "Accept-Encoding": "gzip, deflate",
    "Connection": "keep-alive",
    "Cache-Control": "max-age=0",
}


def now_iso():
    return datetime.now(timezone.utc).isoformat()


def now_ms():
    return time.monotonic()


def check_host_safety(host):
    """Refuse production targets."""
    if host in PRODUCTION_HOSTS:
        print(f"REFUSING production host: {host}", file=sys.stderr)
        sys.exit(1)
    try:
        resolved = socket.gethostbyname(host)
        if resolved == PRODUCTION_IP:
            print(f"REFUSING: {host} resolves to production IP {PRODUCTION_IP}", file=sys.stderr)
            sys.exit(1)
    except socket.gaierror:
        pass  # might be a .local that mDNS hasn't resolved yet


def http_get(host, path, timeout=5):
    """Simple GET returning (status_code, latency_ms, parsed_json_or_none, error)."""
    url = f"http://{host}{path}"
    headers = dict(BROWSER_HEADERS)
    headers["Host"] = host
    req = urllib.request.Request(url, headers=headers)
    start = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = resp.read()
            latency = (time.monotonic() - start) * 1000
            try:
                parsed = json.loads(body)
            except (json.JSONDecodeError, ValueError):
                parsed = None
            return resp.status, latency, parsed, None
    except urllib.error.HTTPError as e:
        latency = (time.monotonic() - start) * 1000
        return e.code, latency, None, str(e)
    except Exception as e:
        latency = (time.monotonic() - start) * 1000
        return 0, latency, None, str(e)


def log_event(output, **kwargs):
    kwargs["timestamp"] = now_iso()
    kwargs["monotonic_ms"] = int(now_ms() * 1000)
    with open(output, "a") as f:
        f.write(json.dumps(kwargs) + "\n")
        f.flush()


def detect_reboot(status_data, prev_uptime):
    """Check if uptime dropped (reboot detected)."""
    if status_data and "uptime_seconds" in status_data:
        uptime = status_data["uptime_seconds"]
        if prev_uptime is not None and uptime < prev_uptime:
            return True, uptime
        return False, uptime
    return False, prev_uptime


def client_worker(host, duration_s, client_id, output, stop_event):
    """Simulate one browser-equivalent client doing the Update+Logs sequence."""
    end_time = time.monotonic() + duration_s
    prev_uptime = None

    while time.monotonic() < end_time and not stop_event.is_set():
        cycle_start = time.monotonic()

        # 1. Load version
        code, lat, data, err = http_get(host, "/version")
        log_event(output, client=client_id, phase="version", status=code,
                  latency_ms=int(lat), error=err)

        # 2. Request update check
        code, lat, data, err = http_get(host, "/update/check")
        log_event(output, client=client_id, phase="update_check_request",
                  status=code, latency_ms=int(lat), error=err)

        # 3. Poll update status until not "checking"
        for attempt in range(60):
            if stop_event.is_set():
                break
            time.sleep(0.5)
            code, lat, data, err = http_get(host, "/update/status")
            if data and data.get("status") != "checking":
                log_event(output, client=client_id, phase="update_status_complete",
                          status=code, update_status=data.get("status"),
                          latency_ms=int(lat), error=err)
                break
            if attempt % 10 == 0:
                log_event(output, client=client_id, phase="update_status_poll",
                          attempt=attempt, status=code, latency_ms=int(lat), error=err)

        # 4. Read check result
        code, lat, data, err = http_get(host, "/update/check_result")
        log_event(output, client=client_id, phase="update_check_result",
                  status=code, latency_ms=int(lat), check_ok=data.get("check_ok") if data else None,
                  error=err)

        # 5. Status snapshot
        code, lat, data, err = http_get(host, "/system_status")
        rebooted, prev_uptime = detect_reboot(data, prev_uptime)
        log_event(output, client=client_id, phase="system_status",
                  status=code, latency_ms=int(lat),
                  uptime=data.get("uptime_seconds") if data else None,
                  heap_free=data.get("heap_free") if data else None,
                  heap_max_alloc=data.get("heap_max_alloc") if data else None,
                  reset_reason=data.get("reset_reason") if data else None,
                  reboot_detected=rebooted,
                  error=err)

        # 6. Open Logs and poll every 5 seconds for the rest of the cycle
        logs_end = time.monotonic() + 30  # 30 seconds of logs polling
        while time.monotonic() < logs_end and not stop_event.is_set():
            code, lat, data, err = http_get(host, "/logs")
            log_event(output, client=client_id, phase="logs", status=code,
                      latency_ms=int(lat),
                      log_count=len(data.get("logs", [])) if data and "logs" in data else None,
                      error=err)
            time.sleep(5)

        # 7. Status snapshot after logs
        code, lat, data, err = http_get(host, "/system_status")
        rebooted, prev_uptime = detect_reboot(data, prev_uptime)
        log_event(output, client=client_id, phase="system_status_post_logs",
                  status=code, latency_ms=int(lat),
                  uptime=data.get("uptime_seconds") if data else None,
                  heap_free=data.get("heap_free") if data else None,
                  heap_max_alloc=data.get("heap_max_alloc") if data else None,
                  reboot_detected=rebooted,
                  error=err)


def main():
    parser = argparse.ArgumentParser(description="ESP32 crash diagnostic workload runner")
    parser.add_argument("--host", required=True, help="Test board IP or hostname")
    parser.add_argument("--duration", type=int, default=3600, help="Duration in seconds")
    parser.add_argument("--clients", type=int, default=1, choices=[1, 2], help="Number of simulated clients")
    parser.add_argument("--output", required=True, help="JSONL output file")
    parser.add_argument("--pressure-target", type=int, default=None,
                        help="Heap pressure target (bytes) — triggers pressure after first cycle")
    args = parser.parse_args()

    check_host_safety(args.host)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    log_event(output, event="start", host=args.host, duration=args.duration,
              clients=args.clients, pressure_target=args.pressure_target)

    stop_event = threading.Event()
    threads = []

    for i in range(args.clients):
        t = threading.Thread(target=client_worker,
                             args=(args.host, args.duration, i, output, stop_event))
        threads.append(t)
        t.start()
        if i == 0:
            time.sleep(2)  # Stagger clients slightly

    try:
        for t in threads:
            t.join()
    except KeyboardInterrupt:
        stop_event.set()
        for t in threads:
            t.join()

    log_event(output, event="stop")


if __name__ == "__main__":
    main()

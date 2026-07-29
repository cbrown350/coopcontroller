# Crash Diagnostics Tools

Bounded reproduction and serial-capture tools for ESPAsyncWebServer
request-header allocation panic investigation.

## Safety

- **Production board `192.168.2.8` is blocked in code** — `run_workload.py`
  hard-refuses that IP and `coopcontroller.local`.
- Only the USB test board at `/dev/cu.usbserial-0001` may be flashed or
  tested.

## serial_capture.py

```bash
python3 tools/crash_diagnostics/serial_capture.py \
  --port /dev/cu.usbserial-0001 \
  --output diagnostics/runs/<run-id>/serial.log
```

Reconnecting serial capture. Appends timestamped lines. Uses PlatformIO's
bundled pyserial.

## run_workload.py

```bash
# One-client realistic workload for 1 hour
python3 tools/crash_diagnostics/run_workload.py \
  --host <test-board-ip> \
  --duration 3600 \
  --clients 1 \
  --output diagnostics/runs/<run-id>/workload.jsonl

# Two-client workload for stress escalation
python3 tools/crash_diagnostics/run_workload.py \
  --host <test-board-ip> \
  --duration 1800 \
  --clients 2 \
  --output diagnostics/runs/<run-id>/workload.jsonl
```

Simulates one or two browser-equivalent clients doing the production-like
Update+Logs interaction. Writes JSONL events with timestamps, HTTP status,
latency, uptime, heap fields, and reboot detection.

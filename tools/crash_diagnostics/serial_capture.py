#!/usr/bin/env python3
"""Reconnecting serial capture for ESP32 diagnostic testing.

Captures timestamped serial output from the USB test board, reconnecting
after disconnects. Uses PlatformIO's bundled pyserial for portability.

Usage:
    python3 serial_capture.py --port /dev/cu.usbserial-0001 --output serial.log

Safety: refuses any port other than /dev/cu.usbserial-0001 unless
--allow-port explicitly matches.
"""
import argparse
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError:
    # Use PlatformIO's bundled pyserial
    import subprocess
    pio_python = str(Path.home() / ".platformio/penv/bin/python")
    sys.path.insert(0, subprocess.check_output(
        [pio_python, "-c", "import serial; import os; print(os.path.dirname(os.path.dirname(serial.__file__)))"],
        text=True).strip())
    import serial

PRODUCTION_HOSTS = {"192.168.2.8", "coopcontroller.local"}
DEFAULT_PORT = "/dev/cu.usbserial-0001"
BAUD = 115200


def main():
    parser = argparse.ArgumentParser(description="ESP32 serial capture with reconnect")
    parser.add_argument("--port", default=DEFAULT_PORT, help="Serial port")
    parser.add_argument("--output", required=True, help="Output log file path")
    parser.add_argument("--allow-port", help="Allow a non-default port (must match)")
    args = parser.parse_args()

    # Safety check
    if args.port != DEFAULT_PORT:
        if not args.allow_port or args.port != args.allow_port:
            print(f"REFUSING port {args.port} — only {DEFAULT_PORT} allowed without --allow-port", file=sys.stderr)
            sys.exit(1)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    running = True

    def handle_sigterm(signum, frame):
        nonlocal running
        running = False
        print("\n[serial_capture] received signal, shutting down...", file=sys.stderr)

    signal.signal(signal.SIGTERM, handle_sigterm)
    signal.signal(signal.SIGINT, handle_sigterm)

    with open(output, "a", buffering=1) as log:
        while running:
            try:
                print(f"[serial_capture] connecting to {args.port}...", file=sys.stderr)
                with serial.Serial(args.port, BAUD, timeout=1) as ser:
                    print(f"[serial_capture] connected", file=sys.stderr)
                    while running:
                        line = ser.readline()
                        if line:
                            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                            decoded = line.decode("utf-8", errors="replace").rstrip()
                            entry = f"[{ts}] {decoded}\n"
                            log.write(entry)
                            sys.stdout.write(entry)
            except (serial.SerialException, OSError) as e:
                print(f"[serial_capture] error: {e}, reconnecting in 2s...", file=sys.stderr)
                time.sleep(2)

    print("[serial_capture] stopped", file=sys.stderr)


if __name__ == "__main__":
    main()

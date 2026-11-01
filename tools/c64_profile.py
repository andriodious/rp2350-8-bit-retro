#!/usr/bin/env python3
"""Capture optional C64 firmware timing windows over USB without polling the web API."""
import argparse
import os
import time
from pathlib import Path

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', default=os.environ.get('PRESTO_PORT'))
    ap.add_argument('--seconds', type=int, default=60)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if args.seconds < 6:
        ap.error('--seconds must be at least 6')
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        ap.error('Install pyserial first (on Debian: sudo apt install python3-serial)')
    port = args.port
    if not port:
        ports = [p.device for p in list_ports.comports() if p.vid == 0x2e8a]
        if len(ports) != 1:
            ap.error('Specify the Presto USB serial port with --port, e.g. /dev/ttyACM0')
        port = ports[0]
    count = 0
    with args.output.open('x') as log, serial.Serial(port, 115200, timeout=1) as usb:
        usb.reset_input_buffer()
        until = time.monotonic() + args.seconds
        print(f'Listening on {port} for {args.seconds} seconds; keep the scene and Turbo setting unchanged.')
        while time.monotonic() < until:
            line = usb.readline().decode('utf-8', errors='replace').strip()
            if not line.startswith(('C64PROF ', 'STATS ')):
                continue
            log.write(line + '\n');log.flush()
            if line.startswith('C64PROF '):
                print(line);count += 1
    if not count:
        raise SystemExit('No timing windows received. Select C64 and flash a C64_PROFILE=ON build; verify the USB port.')
    print(f'Saved {count} windows to {args.output}.')

if __name__ == '__main__':
    main()

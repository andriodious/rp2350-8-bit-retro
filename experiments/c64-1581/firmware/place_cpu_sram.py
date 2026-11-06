#!/usr/bin/env python3
"""Place only the built-in 1581 CPU dispatcher in Pico's startup-copied SRAM.

Keep the Cargo archive intact. Match the semantic symbol components rather than
Rust's compiler-dependent hash; fail closed if the code shape changes.
"""
import argparse
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--objdump', required=True)
p.add_argument('--objcopy', required=True)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if a.input.resolve() == a.output.resolve():
    p.error('input and output must differ')
listing = subprocess.check_output([a.objdump, '-h', str(a.input)], text=True)
matches = []
for line in listing.splitlines():
    m = re.match(r'\s*\d+\s+(\.text\.\S+)\s+([0-9a-fA-F]+)\s', line)
    if m and 'drive_6510core_execute' in m[1] and '7BuiltIn' in m[1]:
        matches.append((m[1], int(m[2], 16)))
if len(matches) != 1:
    raise SystemExit(f'Expected exactly one built-in dispatcher, found {len(matches)}')
section, size = matches[0]
if not 0 < size <= 10 * 1024:
    raise SystemExit(f'Dispatcher SRAM budget exceeded: {size} bytes (limit 10240)')
subprocess.run([a.objcopy, '--rename-section',
                section + '=.time_critical.drive1581_cpu',
                str(a.input), str(a.output)], check=True)
print(f'1581 built-in CPU dispatcher: {size} bytes placed in SRAM')

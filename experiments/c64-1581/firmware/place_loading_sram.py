#!/usr/bin/env python3
"""Relocate the drive scheduler and built-in mapped reads, preserving opcodes.

Only section names change. Reject unexpected compiler output and enforce a
4 KiB code budget; leave the large instruction dispatcher and writes in flash.
"""
import argparse
from pathlib import Path
import re
import subprocess


def select(listing):
    groups = {'scheduler': [], 'read': []}
    for line in listing.splitlines():
        m = re.match(r'\s*\d+\s+(\.text\.\S+)\s+([0-9a-fA-F]+)\s', line)
        if not m:
            continue
        name, size = m[1], int(m[2], 16)
        if '9Drive158110run_cycles' in name:
            groups['scheduler'].append((name, size))
        if '7BuiltIn' in name and '11read_mapped' in name:
            groups['read'].append((name, size))
    for kind, matches in groups.items():
        if len(matches) != 1:
            raise ValueError(f'Expected one {kind} section, found {len(matches)}')
    sections = {kind: matches[0] for kind, matches in groups.items()}
    if any(size == 0 for _, size in sections.values()) or sum(size for _, size in sections.values()) > 4096:
        raise ValueError('Loading SRAM code exceeds 4096-byte budget or is empty')
    return sections


def main():
    p = argparse.ArgumentParser()
    for name in ('objdump', 'objcopy', 'input', 'output'):
        p.add_argument('--' + name, required=True)
    a = p.parse_args()
    if Path(a.input).resolve() == Path(a.output).resolve():
        p.error('input and output must differ')
    sections = select(subprocess.check_output([a.objdump, '-h', a.input], text=True))
    args = [a.objcopy]
    for kind, (name, size) in sections.items():
        args += ['--rename-section', name + '=.time_critical.drive1581_' + kind]
        print(f'Drive {kind}: {size} bytes to SRAM')
    subprocess.run(args + [a.input, a.output], check=True)


if __name__ == '__main__':
    main()

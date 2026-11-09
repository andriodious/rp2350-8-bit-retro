"""Check archive relocation integrity and final linked SRAM placement.
Usage: python3 ... ELF INPUT_ARCHIVE OUTPUT_ARCHIVE arm-none-eabi-objdump
"""
import importlib.util
from pathlib import Path
import re
import subprocess
import sys

script = Path(__file__).resolve().parents[2] / 'experiments/c64-1581/firmware/place_loading_sram.py'
spec = importlib.util.spec_from_file_location('placement', script)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
elf, original, relocated, objdump = sys.argv[1:]
listing = subprocess.check_output([objdump, '-h', original], text=True)
sections = m.select(listing)
# Missing, duplicate and over-budget sections must fail closed.
for invalid in ['', listing + listing, listing.replace('000005ce', '00005000')]:
    if invalid == listing:
        continue
    try:
        m.select(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted invalid section inventory')

def contents(archive, section):
    text = subprocess.check_output([objdump, '-s', '-j', section, archive], text=True)
    result = bytearray()
    for line in text.splitlines():
        if re.match(r'^ [0-9a-f]+ ', line):
            # objdump's 16-byte hex column precedes printable ASCII.
            result.extend(bytes.fromhex(line.split(' ', 2)[2][:35].replace(' ', '')))
    return bytes(result)

symbols = subprocess.check_output([objdump, '-t', elf], text=True)
for kind, (section, size) in sections.items():
    target = '.time_critical.drive1581_' + kind
    before, after = contents(original, section), contents(relocated, target)
    assert len(before) == size, (kind, len(before), size)
    assert before == after, kind
    symbol = section.removeprefix('.text.')
    lines = [line for line in symbols.splitlines() if line.endswith(' ' + symbol)]
    assert len(lines) == 1, symbol
    assert 0x20000000 <= int(lines[0].split()[0], 16) < 0x2007c000, lines[0]
# The general built-in dispatcher must still be in flash.
lines = [line for line in symbols.splitlines() if 'drive_6510core_execute' in line and '7BuiltIn' in line and not line.endswith('_veneer')]
assert len(lines) == 1 and 0x10000000 <= int(lines[0].split()[0],16) < 0x11000000
print('Loading sections unchanged byte-for-byte and linked in SRAM; dispatcher remains in flash')

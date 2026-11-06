"""Check the diagnostic ROM caller and LED path in the linked launcher ELF.
Usage: python3 tests/startup/partition_placement_test.py ELF arm-none-eabi-objdump
"""
import re
import subprocess
import sys
text = subprocess.check_output([sys.argv[2], '-d', sys.argv[1]], text=True)
blocks = re.split(r'\n(?=[0-9a-f]+ <)', text)
found = set()
for block in blocks:
    match = re.match(r'([0-9a-f]+) <(partition_(?:call|led|stop)(?:\.[^>]*)?)>:', block)
    if not match:
        continue
    name = match[2].split('.')[0]
    found.add(name)
    assert 0x20078000 <= int(match[1], 16) < 0x20080000, match[2]
    # Direct calls must stay in the inspected RAM-only helper family. Two
    # indirect BLX calls in partition_call are the ROM lookup and ROM API.
    for target, symbol in re.findall(r'\bbl\s+([0-9a-f]+) <([^>]+)>', block):
        assert 0x20078000 <= int(target, 16) < 0x20080000, symbol
        assert symbol.startswith(('partition_led', 'partition_stop')), symbol
    for literal in re.findall(r'\.word\s+0x([0-9a-f]+)', block):
        assert not 0x10000000 <= int(literal, 16) < 0x20000000, literal
    if name == 'partition_call':
        assert len(re.findall(r'\bblx\s+r\d+', block)) == 2
assert found == {'partition_call', 'partition_led', 'partition_stop'}, found
print('ROM caller, LED helpers and literal pools are in SRAM; no direct flash calls')

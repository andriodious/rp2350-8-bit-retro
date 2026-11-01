"""Verify the linked experiment, not just its CMake option.
Usage: python3 tests/c64/ram_placement_test.py /path/arm-none-eabi-nm firmware.elf on|off [cpu-on]
"""
import subprocess,sys
text=subprocess.check_output([sys.argv[1],'-C',sys.argv[2]],text=True)
symbols=[]
for line in text.splitlines():
    fields=line.split(maxsplit=2)
    if len(fields)==3:
        try:address=int(fields[0],16)
        except ValueError:continue
        symbols.append((address,fields[1],fields[2]))
ram=sys.argv[3]=='on'
vic=[(a,n) for a,t,n in symbols if t in ('T','t') and n.startswith('MOS6569::')]
assert any('EmulateLine' in n for a,n in vic)
for a,n in vic:
    assert (0x20000000<=a<0x20080000) if ram else (0x10000000<=a<0x11000000),(hex(a),n)
cpu_ram=len(sys.argv)>4 and sys.argv[4]=='cpu-on'
cpu=[a for a,t,n in symbols if n=='MOS6510::EmulateLine(int)'];assert len(cpu)==1
assert (0x20000000<=cpu[0]<0x20080000) if cpu_ram else (0x10000000<=cpu[0]<0x11000000)
lookup={n:a for a,t,n in symbols}
headroom=lookup['__HeapLimit']-lookup['__end__']
# Explicitly budget this higher-memory experiment; these are link-time guards,
# not proof of sufficient live heap after Wi-Fi allocations.
assert headroom >= (46*1024 if cpu_ram else 65536),headroom
print(f'VIC code in {"SRAM" if ram else "flash"}; CPU in {"SRAM" if cpu_ram else "flash"}; {headroom} bytes before runtime heap allocations')

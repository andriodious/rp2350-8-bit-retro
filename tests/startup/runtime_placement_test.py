"""Verify pre-main probe order and CRT wrapper wiring in the built ELF.
Usage: python3 tests/startup/runtime_placement_test.py ELF arm-none-eabi-nm arm-none-eabi-objdump
"""
import re, struct, subprocess, sys
from pathlib import Path
elf=Path(sys.argv[1]);data=elf.read_bytes()
text=subprocess.check_output([sys.argv[2],str(elf)],text=True)
symbols={p[2]:int(p[0],16) for line in text.splitlines() if len(p:=line.split())==3}
start,end=symbols['__preinit_array_start'],symbols['__preinit_array_end']
phoff=struct.unpack_from('<I',data,28)[0];size,count=struct.unpack_from('<HH',data,42)
entries=None
for i in range(count):
 kind,offset,vaddr,_,filesz,_,_,_=struct.unpack_from('<8I',data,phoff+i*size)
 if kind==1 and vaddr<=start and end<=vaddr+filesz:
  entries=[x&~1 for x in struct.unpack_from('<'+'I'*((end-start)//4),data,offset+start-vaddr)]
assert entries is not None
order=['runtime_init_clocks','runtime_init_spin_locks_reset','runtime_init_mutex','runtime_init_default_alarm_pool','runtime_init_per_core_enable_coprocessors','runtime_init_per_core_irq_priorities','startup_runtime_ready']
indices=[entries.index(symbols[s]&~1) for s in order]
assert indices==sorted(indices),list(zip(order,indices))
dis=subprocess.check_output([sys.argv[3],'-d',str(elf)],text=True)
# CRT uses an indirect call through a literal pointer to main. Linker
# wrapping must redirect that literal, not merely leave an unused wrapper.
addr=symbols['__wrap_main']|1
assert re.search(r'\.word\s+0x'+f'{addr:08x}',dis), 'CRT wrapper pointer missing'
block=re.search(r'^[0-9a-f]+ <__wrap_main>:\n(.*?)(?=\n[0-9a-f]+ <)',dis,re.M|re.S)
assert block and re.search(r'\b(?:bl|b\.w)\s+[0-9a-f]+ <main>',block[1])
print(elf.name+': preinit order and CRT runtime wrapper verified')

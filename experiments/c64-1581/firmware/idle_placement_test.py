#!/usr/bin/env python3
"""Verify the experimental idle handler is startup-loaded SRAM, with bounded cost."""
import argparse, subprocess, struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--nm',required=True);a=p.parse_args()
listing=subprocess.check_output([a.nm,'-S','-C',str(a.elf)],text=True)
handlers=[];dispatchers=[];symbols={}
for line in listing.splitlines():
 fields=line.split()
 if len(fields)==4 and fields[2] in ['t','T']:
  addr,size=int(fields[0],16),int(fields[1],16)
  if '::skip_data_wait::' in fields[3] and 'BuiltIn' in fields[3]:handlers.append((addr,size))
  if '::drive_6510core_execute::' in fields[3]:dispatchers.append(addr)
 if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
assert len(handlers)==1,handlers
addr,size=handlers[0]
assert 0x20000000<=addr<addr+size<=0x20080000 and 0<size<=1536
assert dispatchers and all(0x10000000<=x<0x11000000 for x in dispatchers)
headroom=symbols['__HeapLimit']-symbols['__end__']
assert headroom>=33728-1536,headroom
# The handler must be file-backed in the ELF, loaded from flash into SRAM.
b=a.elf.read_bytes();phoff=struct.unpack_from('<I',b,28)[0];phsize,phnum=struct.unpack_from('<HH',b,42)
loaded=False
for i in range(phnum):
 kind,off,vaddr,paddr,filesz,memsz,flags,align=struct.unpack_from('<8I',b,phoff+i*phsize)
 if kind==1 and vaddr<=addr and addr+size<=vaddr+filesz:
  assert 0x10000000<=paddr<0x11000000
  assert off+filesz<=len(b)
  loaded=True
assert loaded
print(f'Idle handler: {size} bytes in startup-loaded SRAM; dispatcher in flash; static heap headroom {headroom} bytes')

#!/usr/bin/env python3
"""Decode a diagnostic status JSON using the exact candidate's retained ELF."""
import argparse,json,subprocess
p=argparse.ArgumentParser();p.add_argument('status');p.add_argument('elf');p.add_argument('--prefix',default='arm-none-eabi-');a=p.parse_args()
s=json.load(open(a.status));f=s.get('c64_failure',{})
kinds={1:'abort',2:'exit',3:'_exit',4:'assertion',5:'panic',6:'hard assertion'}
stages={0:'outside frontend',1:'BOOT/touch menu',2:'preferences restore',3:'USB polling',4:'API frame',5:'drive/save frame',6:'network frame',7:'Bluetooth frame',8:'controller frame'}
if f.get('reason')!='terminal-call':p.error('No terminal-call record; preserve raw status for watchdog/hardfault analysis')
print(kinds.get(f['kind'],'unknown'),';',stages.get(f['frontend_stage'],'unknown'),'core',f['core'],'phase',f['phase'],'line',f['line'])
pc=int(f['caller'],16)
# LR is the next Thumb instruction, not the call instruction itself.
subprocess.run([a.prefix+'addr2line','-f','-C','-e',a.elf,hex((pc&~1)-2)],check=True)
if f['kind'] in (4,5):
 detail=int(f['detail'],16)
 if 0x10000000<=detail<0x11000000:
  print('Saved assertion expression / panic format bytes:')
  subprocess.run([a.prefix+'objdump','-s','--start-address='+hex(detail),'--stop-address='+hex(detail+160),a.elf],check=True)

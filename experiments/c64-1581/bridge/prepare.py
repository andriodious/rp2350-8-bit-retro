#!/usr/bin/env python3
"""Patch a disposable copy only; leave the checksum-verified firmware core intact."""
from pathlib import Path
import shutil,sys
root=Path(__file__).resolve().parents[3]
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
core=out/'core'
if core.exists():raise SystemExit(f'{core} already exists; use a fresh output directory')
shutil.copytree(root/'deps/frank-c64/src',core)
def change(name,old,new):
 p=core/name;s=p.read_text();assert s.count(old)==1,(name,s.count(old));p.write_text(s.replace(old,new))
change('rp2350/C64_rp2350.cpp','Kernal = c64_fast_reset_rom;','Kernal = BuiltinKernalROM;')
change('CPUC64.cpp','#include "CPUC64.h"','#include "drive_bridge.h"\n#define RETRO_DRIVE_CPU_HOOK 1\n#include "CPUC64.h"')
change('CPU_emulline.h','\t\t\tlast_cycles += page_cycles;','\t\t\tlast_cycles += page_cycles;\n#ifdef RETRO_DRIVE_CPU_HOOK\n            retro_drive_tick(last_cycles);\n#endif')
change('CIA.cpp','#include "sysdeps.h"','#include "sysdeps.h"\n#include "drive_bridge.h"')
change('CIA.cpp','((the_cpu_1541->CalcIECLines() & 0x30) << 2)','(retro_drive_read() & 0xc0)')
change('CIA.cpp','\tuint8_t old_lines = IECLines;','\tretro_drive_write(~inv_out);\n\tuint8_t old_lines = IECLines;')
change('rp2350/C64_rp2350.cpp','#include "../IEC.h"','#include "../IEC.h"\n#include "drive_bridge.h"')
change('rp2350/C64_rp2350.cpp','C64_PROFILE_CALL(CPU, c64->TheCPU->EmulateLine(cycles_left));','retro_drive_tick(CYCLES_PER_LINE - cycles_left);\n        C64_PROFILE_CALL(CPU, c64->TheCPU->EmulateLine(cycles_left));\n        retro_drive_flush();')
change('rp2350/ROM_data.h', 'extern const uint8_t BuiltinCharROM', 'extern const uint8_t BuiltinKernalROM[8192];\nextern const uint8_t BuiltinCharROM')
change('rp2350/ROM_data.cpp', '// Kernal ROM\n/*', '// Kernal ROM')
change('rp2350/ROM_data.cpp', '*/\n// Character ROM', '// Character ROM')
print(core)

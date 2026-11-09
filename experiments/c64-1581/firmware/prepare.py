#!/usr/bin/env python3
"""Prepare optional 1581 firmware sources in the CMake build directory."""
import argparse, json, shutil, subprocess, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--trx64',type=Path,required=True)
p.add_argument('--idle-sram',action='store_true')
p.add_argument('--drive-profile',action='store_true')
p.add_argument('--flush-lines',type=int,choices=[1,4,8,16],default=1)
a=p.parse_args();out=a.output.resolve()
if not a.trx64.exists():
 subprocess.run(['git','clone','https://github.com/Jondalar/TRX64',str(a.trx64)],check=True)
 subprocess.run(['git','-C',str(a.trx64),'checkout','--detach','a86341c8b2add9841dab1a8340fb76d71121e1e3'],check=True)
if out.exists():
 if not (out/'.generated-1581').is_file():raise SystemExit('Refusing to replace unmarked output')
 shutil.rmtree(out)
out.mkdir(parents=True);(out/'.generated-1581').touch()
subprocess.run([sys.executable,str(HERE.parent/'bridge/prepare.py'),str(out)],check=True)
subprocess.run([sys.executable,str(HERE.parent/'portable/prepare.py'),'--trx64',str(a.trx64),'--output',str(out/'portable')],check=True)
core=out/'core'
def change(name,old,new):
 f=core/name;s=f.read_text();assert s.count(old)==1,(name,s.count(old));f.write_text(s.replace(old,new))
change('rp2350/C64_rp2350.cpp','Kernal = BuiltinKernalROM;','Kernal = c64_fast_reset_rom;')
change('CPUC64.h','\tconst uint8_t * kernal_rom;', '\tconst uint8_t * kernal_rom;') # existence guard
# Explicit setter keeps the CPU ROM pointer and C64 owner in step on media changes.
change('CPUC64.h','\tMOS6510(C64 * c64,', '\tvoid SetKernal(const uint8_t *rom) { kernal_rom = rom; }\n\tMOS6510(C64 * c64,')
change('CPU_emulline.h','retro_drive_tick(last_cycles);','retro_drive_add_cycles(last_cycles);')
change('CIA.cpp','(retro_drive_read() & 0xc0)','(retro_drive_active() ? (retro_drive_read() & 0xc0) : ((the_cpu_1541->CalcIECLines() & 0x30) << 2))')
change('CIA.cpp','retro_drive_write(~inv_out);','if (retro_drive_active()) retro_drive_write(~inv_out);')
change('rp2350/C64_rp2350.cpp','retro_drive_tick(CYCLES_PER_LINE - cycles_left);','retro_drive_add_cycles(CYCLES_PER_LINE - cycles_left);')
# Every serial read/write still catches up immediately in the bridge. Only
# otherwise-unobserved scanline flushes are batched; frames always end caught up.
change('rp2350/C64_rp2350.cpp','retro_drive_flush();',
       f'if (retro_drive_active() && (((line_count + 1) % {a.flush_lines}) == 0 || (vic_flags & VIC_VBLANK) || line_count + 1 == MAX_LINES_PER_FRAME)) retro_drive_flush();')
# Poll transport after completed CPU/drive work, never from a Rust borrow or opcode.
change('rp2350/C64_rp2350.cpp','        line_count++;', '        line_count++;\n#if C64_1581_NET_SLICES\n        if (retro_drive_enabled) retro_drive_network_poll();\n#endif')
change('rp2350/C64_rp2350.cpp','void C64::Reset(bool clear_memory)\n{','void C64::Reset(bool clear_memory)\n{\n    retro_drive_reset();')
change('rp2350/C64_rp2350.cpp','    TheC64->MountDrive8(false, filename);','''    retro_drive_close();
    if (data && size == 819200 && retro_drive_init(retro_1581_rom,32768,data,size)) {
        TheC64->UnmountDrive8();
        TheC64->Kernal = BuiltinKernalROM;
        TheC64->TheCPU->SetKernal(TheC64->Kernal);
        return;
    }
    TheC64->Kernal = c64_fast_reset_rom;
    TheC64->TheCPU->SetKernal(TheC64->Kernal);
    TheC64->MountDrive8(false, filename);''')
change('rp2350/C64_rp2350.cpp','void c64_unmount_disk(void) {','''void c64_unmount_disk(void) {
    retro_drive_close();
    if (TheC64) {TheC64->Kernal=c64_fast_reset_rom;TheC64->TheCPU->SetKernal(TheC64->Kernal);}''')
rust=out/'rust';(rust/'src').mkdir(parents=True)
(rust/'src/lib.rs').write_bytes((HERE/'lib.rs').read_bytes())
(rust/'Cargo.toml').write_text('''[package]
name="retro-1581-firmware"
version="0.0.1"
edition="2021"
license="GPL-3.0-or-later"
[lib]
crate-type=["staticlib"]
[profile.release]
panic="abort"
opt-level=2
codegen-units=1
[dependencies]
retro-drive1581={path="../portable"}
''')

if a.idle_sram:
 p=rust/'Cargo.toml'
 p.write_text(p.read_text().replace('retro-drive1581={path="../portable"}', 'retro-drive1581={path="../portable",features=["idle-sram"]}'))

if a.drive_profile:
 p=rust/'Cargo.toml'
 p.write_text(p.read_text()+'\n[features]\ndrive-profile=["retro-drive1581/drive-profile"]\n')
 # Enable instrumentation for this generated diagnostic crate only.
 p.write_text(p.read_text().replace('[features]', '[features]\ndefault=["drive-profile"]'))

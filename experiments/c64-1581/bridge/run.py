#!/usr/bin/env python3
"""Build the desktop Frodo + TRX64 1581 integration in a disposable directory."""
import argparse, hashlib, json, os, platform, runpy, subprocess, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--trx64',type=Path,required=True)
p.add_argument('--portable',action='store_true',help='Use the extracted no_std drive subset')
p.add_argument('--image',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();trx=a.trx64.resolve()
if subprocess.check_output(['git','-C',str(trx),'rev-parse','HEAD'],text=True).strip()!='a86341c8b2add9841dab1a8340fb76d71121e1e3':p.error('Wrong TRX64 revision')
subprocess.run(['git','-C',str(trx),'diff','--exit-code','HEAD'],check=True)
if not a.image.is_file() or a.image.stat().st_size!=819200:p.error('Expected an 819200-byte D81')
# Do not silently test an edited dependency tree.
config=json.loads((ROOT/'third_party/c64-sources.json').read_text())
for name,digest in config['frank_files'].items():
 if hashlib.sha256((ROOT/'deps/frank-c64/src'/name).read_bytes()).hexdigest()!=digest:p.error('Modified FRANK dependency: '+name)
rom=runpy.run_path(str(ROOT/'scripts/fetch-1581-rom.py'))['fetch']()
subprocess.run([sys.executable,str(HERE/'prepare.py'),str(out)],check=True)
rust=out/'rust';(rust/'src').mkdir(parents=True)
(rust/'src/lib.rs').write_bytes((HERE.parent/'portable/host_bridge.rs' if a.portable else HERE/'lib.rs').read_bytes())
(rust/'Cargo.lock').write_bytes((trx/'Cargo.lock').read_bytes())
(rust/'Cargo.toml').write_text('[package]\nname="frodo-1581-bridge"\nversion="0.1.0"\nedition="2021"\nlicense="GPL-3.0-or-later"\n[lib]\ncrate-type=["staticlib"]\n[dependencies]\ntrx64-core={path='+json.dumps(str(trx/'crates/trx64-core'))+'}\n')
if a.portable:
 subprocess.run([sys.executable,str(HERE.parent/'portable/prepare.py'),'--trx64',str(trx),'--output',str(out/'portable')],check=True)
 (rust/'Cargo.toml').write_text('[package]\nname="frodo-1581-bridge"\nversion="0.1.0"\nedition="2021"\nlicense="GPL-3.0-or-later"\n[lib]\ncrate-type=["staticlib"]\n[dependencies]\nretro-drive1581={path='+json.dumps(str(out/'portable'))+'}\n')
env=dict(os.environ);env['CARGO_TARGET_DIR']=str(rust/'target')
subprocess.run([os.environ.get('CARGO','cargo'),'build','--release','--manifest-path',str(rust/'Cargo.toml')],env=env,check=True)
core=out/'core';machine=ROOT/'src/machines/c64';host=ROOT/'tests/c64/native'
sanitize=['-fsanitize=address,undefined']
for name in ['disk_file','t64']:
 subprocess.run([os.environ.get('CC','cc'),*sanitize,'-I'+str(machine),'-c',str(machine/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
sources=[HERE/'main.cpp',host/'arch.cpp',out/'disk_file.o',out/'t64.o',machine/'display.cpp',machine/'crt.cpp']
sources += [core/'rp2350'/(n+'.cpp') for n in ['C64_rp2350','Prefs_rp2350','ROM_data','SID_rp2350','Tape_stub']]
sources += [core/(n+'.cpp') for n in ['CPUC64','VIC','CIA','IEC','Cartridge','REU','1541d64','1541gcr','CPU1541','VIA']]
libs=['-framework','Security','-framework','CoreFoundation','-liconv'] if platform.system()=='Darwin' else ['-ldl','-lpthread','-lm']
subprocess.run([os.environ.get('CXX','c++'),'-std=c++20','-O1','-g',*sanitize,'-DFRODO_RP2350','-DPSRAM_MAX_FREQ_MHZ=125',*['-I'+str(d) for d in [HERE,host,machine,core,core/'rp2350']],*map(str,sources),str(rust/'target/release/libfrodo_1581_bridge.a'),*libs,'-o',str(out/'test')],check=True)
env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
before=hashlib.sha256(a.image.read_bytes()).digest()
try:
 for mode in ['probe','game']:
  subprocess.run([str(out/'test'),str(rom),str(a.image.resolve()),str(out),mode],env=env,check=True)
finally:
 if hashlib.sha256(a.image.read_bytes()).digest()!=before:raise RuntimeError('Input D81 changed')
print('Probe passed. Inspect the game snapshots; frame completion is not a compatibility pass.')

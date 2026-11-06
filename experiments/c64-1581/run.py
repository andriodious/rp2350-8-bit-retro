#!/usr/bin/env python3
"""Build a pinned desktop reference, with ROMs and game files supplied separately."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import runpy

PIN = 'a86341c8b2add9841dab1a8340fb76d71121e1e3'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--trx64', type=Path, required=True)
p.add_argument('--rom-dir', type=Path, required=True)
p.add_argument('--image', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--frames', type=int, default=10000)
a = p.parse_args()
if not 500 <= a.frames <= 100000:
    p.error('--frames must be between 500 and 100000')
root = a.trx64.resolve()
sha = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
if sha != PIN:
    p.error(f'TRX64 must be checked out at {PIN}, got {sha}')
subprocess.run(['git', '-C', str(root), 'diff', '--exit-code', 'HEAD'], check=True)
# Fetch only for the experimental full-drive build, not normal firmware.
rom_setup = runpy.run_path(str(Path(__file__).resolve().parents[2] / 'scripts/fetch-1581-rom.py'))
rom_target = a.rom_dir / rom_setup['NAME']
if rom_target.exists():
    rom_setup['verify'](rom_target)
else:
    cached = rom_setup['fetch']()
    rom_setup['fetch'](rom_target, cached)
for name, size in [('basic-901226-01.bin', 8192), ('kernal-901227-03.bin', 8192),
                   ('chargen-901225-01.bin', 4096), ('dos1541-325302-01+901229-05.bin', 16384),
                   ('dos1581-318045-02.bin', 32768)]:
    file = a.rom_dir / name
    if not file.is_file() or file.stat().st_size != size:
        p.error(f'Missing/wrong-size ROM: {file} (expected {size} bytes)')
if not a.image.is_file() or a.image.stat().st_size not in (819200, 822400):
    p.error('Expected an 80-track D81, with optional error bytes')
out = a.output.resolve()
build = out / 'reference-build'
(build / 'src').mkdir(parents=True, exist_ok=True)
(build / 'src/main.rs').write_text(Path(__file__).with_name('reference.rs').read_text())
(build / 'Cargo.toml').write_text('[package]\nname="retro-1581-reference"\nversion="0.1.0"\nedition="2021"\nlicense="GPL-3.0-or-later"\n[dependencies]\ntrx64-core={path=' + json.dumps(str(root / 'crates/trx64-core')) + '}\n')
# Lock transitive versions to the reference project's reviewed lockfile. Cargo
# adds this small runner package; it must not upgrade the pinned dependencies.
if not (build / 'Cargo.lock').exists():
    (build / 'Cargo.lock').write_bytes((root / 'Cargo.lock').read_bytes())
subprocess.run([os.environ.get('CARGO', 'cargo'), 'build', '--release', '--manifest-path', str(build/'Cargo.toml')], check=True)
before = hashlib.sha256(a.image.read_bytes()).digest()
try:
    subprocess.run([str(build/'target/release/retro-1581-reference'), str(a.rom_dir.resolve()), str(a.image.resolve()), str(out), str(a.frames)], check=True)
finally:
    if hashlib.sha256(a.image.read_bytes()).digest() != before:
        raise RuntimeError('The input D81 changed during the reference run')
print('PASS: input D81 checksum unchanged')

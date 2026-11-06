#!/usr/bin/env python3
"""Compare the guarded DATA-wait shortcut with the original interpreter."""
import argparse
from pathlib import Path
import subprocess
import sys
p=argparse.ArgumentParser()
p.add_argument('--trx64',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
subprocess.run([sys.executable,str(Path(__file__).with_name('prepare.py')),
                '--trx64',str(a.trx64),'--output',str(a.output)],check=True)
examples=a.output/'examples';examples.mkdir()
(examples/'idle.rs').write_text('fn main(){retro_drive1581::drive1581::idle_validation::validate();println!("Idle differential tests passed");}\n')
subprocess.run(['cargo','run','--release','--manifest-path',str(a.output/'Cargo.toml'),
                '--features','idle-validation,idle-sram','--example','idle'],check=True)

#!/usr/bin/env python3
"""Exercise the actual single-core sampler with a wrapping deterministic clock."""
from pathlib import Path
import subprocess, tempfile
source=Path(__file__).with_name('profile.rs').resolve()
with tempfile.TemporaryDirectory(prefix='drive-profile-') as tmp:
 p=Path(tmp)/'test.rs'
 p.write_text('''use std::sync::atomic::{AtomicU32,Ordering};
static CLOCK:AtomicU32=AtomicU32::new(u32::MAX-10);
#[no_mangle] extern "C" fn retro_drive_time_us()->u32 {CLOCK.fetch_add(1,Ordering::Relaxed)}
#[path="'''+str(source)+'''"] mod profile;
fn main(){
 profile::reset();
 for _ in 0..100000 {profile::begin();profile::mark(1);profile::mark(2);profile::mark(3);profile::end();}
 assert_eq!(profile::get(0),100000);
 let samples=profile::get(1); assert!(samples>300 && samples<500);
 for i in 2..6 {assert_eq!(profile::get(i),samples);}
 assert_eq!(CLOCK.load(Ordering::Relaxed),(u32::MAX-10).wrapping_add(samples as u32*5));
 profile::reset();for i in 0..6 {assert_eq!(profile::get(i),0);}
 println!("Sampler: accounting, sampling rate, reset and clock wrap passed ({samples} samples)");
}
''')
 subprocess.run(['rustc','--edition=2021',str(p),'-o',str(Path(tmp)/'test')],check=True)
 subprocess.run([str(Path(tmp)/'test')],check=True)

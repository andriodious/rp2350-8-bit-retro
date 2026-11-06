// SPDX-License-Identifier: GPL-3.0-or-later
// Desktop-only reference experiment. Uses TRX64's public API; no firmware linkage.
// Boot/keyboard approach follows Jondalar/TRX64 drive1581_gate.rs (GPL-3.0-or-later).
use std::{env, fs, path::Path, time::Instant};
use trx64_core::{Machine, NullSink};
use trx64_core::drive::{DiskImage, DiskKind, DrivePosition};
use trx64_core::iec::DriveType;
const FRAME: u64 = 19_656;
fn frames(m: &mut Machine, n: usize) {
    for _ in 0..n { m.run_for_full(FRAME, &mut NullSink, |_,_,_,_,_,_,_| {}); }
}
fn type_in(m: &mut Machine, text: &[u8]) {
    for chunk in text.chunks(10) {
        let mut ready=false;
        for _ in 0..250 { if m.read_full(198)==0 {ready=true;break;} frames(m,1); }
        assert!(ready,"C64 keyboard buffer did not drain");
        m.poke(631,chunk);m.poke(198,&[chunk.len() as u8]);frames(m,2);
    }
}
fn snapshot(m: &Machine, out: &Path, frame: usize) {
    let (w,h,p)=m.render_canvas_rgba();
    let mut ppm=format!("P6\n{w} {h}\n255\n").into_bytes();
    for q in p.chunks_exact(4) {ppm.extend_from_slice(&q[..3]);}
    fs::write(out.join(format!("frame-{frame:05}.ppm")),ppm).unwrap();
    fs::write(out.join(format!("ram-{frame:05}.bin")),&m.ram[..]).unwrap();
}
fn run() {
    let a:Vec<String>=env::args().collect();
    assert!(a.len()>=4,"usage: reference ROM_DIR D81 OUTPUT_DIR [FRAMES]");
    let rom=Path::new(&a[1]);let out=Path::new(&a[3]);fs::create_dir_all(out).unwrap();
    let mut m=Machine::new();m.boot_from_dir(rom).expect("C64 ROMs");
    m.set_drive_power(DrivePosition::A,false).unwrap();
    m.set_drive_type(DrivePosition::A,DriveType::Drive1581).expect("1581 ROM");
    m.drive8.mount(DiskImage{kind:DiskKind::D81,bytes:fs::read(&a[2]).unwrap(),backing_path:None,read_only:true}).unwrap();
    m.set_drive_power(DrivePosition::A,true).unwrap();frames(&mut m,200);
    // Upload a real 6502 program at $0500 through the C64's serial bus.
    // It writes a marker at $0700 and returns to DOS. U3 must execute it.
    let program=[
        "10 OPEN15,8,15:FORI=0TO5:READA:PRINT#15,\"M-W\"CHR$(I)CHR$(5)CHR$(1)CHR$(A):NEXT",
        "20 PRINT#15,\"U3\":CLOSE15",
        "30 DATA169,90,141,0,7,96",
    ];
    for line in program {type_in(&mut m,format!("{line}\r").as_bytes());frames(&mut m,5);}
    type_in(&mut m,b"RUN\r");frames(&mut m,250);
    assert_eq!(m.drive8.drive_peek(0x700),90,"U3 did not execute uploaded code");
    println!("PASS: M-W + U3 executed uploaded drive code; $0700=$5a");
    // Boot a fresh machine to avoid the probe changing the game's drive state.
    drop(m);let mut m=Machine::new();m.boot_from_dir(rom).unwrap();
    m.set_drive_power(DrivePosition::A,false).unwrap();
    m.set_drive_type(DrivePosition::A,DriveType::Drive1581).unwrap();
    m.drive8.mount(DiskImage{kind:DiskKind::D81,bytes:fs::read(&a[2]).unwrap(),backing_path:None,read_only:true}).unwrap();
    m.set_drive_power(DrivePosition::A,true).unwrap();frames(&mut m,200);
    type_in(&mut m,b"LOAD\"*\",8,1\rRUN\r");
    let count=a.get(4).map(|s|s.parse().unwrap()).unwrap_or(10000);
    let start=Instant::now();
    for f in 1..=count {
        frames(&mut m,1);
        if f%500==0 {snapshot(&m,out,f);println!("frame={f} elapsed_ms={} drive_pc={:04x}",start.elapsed().as_millis(),m.drive8.cpu().reg_pc);}
    }
    println!("Completed {count} frames. Inspect snapshots; completion alone is not a compatibility pass.");
}
fn main() {std::thread::Builder::new().stack_size(16*1024*1024).spawn(run).unwrap().join().unwrap();}

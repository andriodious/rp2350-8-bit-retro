// SPDX-License-Identifier: GPL-3.0-or-later
// Desktop-only C ABI adapter to the pinned TRX64 drive implementation.
use std::cell::RefCell;
use trx64_core::drive::{Drive1541,DiskImage,DiskKind,pair_catch_up,pair_fold_into_iec,pair_deliver_atn_edge};
use trx64_core::iec::{IecCore,DriveType};
struct Bridge { a:Drive1541,b:Drive1541,iec:IecCore,clock:u64,last:u64,pa:u8 }
impl Bridge {
 fn catch_up(&mut self,target:u64) {self.last=pair_catch_up(&mut self.a,&mut self.b,&mut self.iec,target,self.last,self.pa);}
 fn fold(&mut self) {pair_fold_into_iec(&self.a,&self.b,&mut self.iec,self.pa);}
}
thread_local! {static DRIVE:RefCell<Option<Bridge>>=const {RefCell::new(None)};}
#[no_mangle]
pub unsafe extern "C" fn retro_drive_init(rom:*const u8,rom_len:usize,disk:*const u8,disk_len:usize)->bool {
 if rom.is_null() || disk.is_null() || rom_len!=32768 || disk_len!=819200 {return false;}
 let mut a=Drive1541::new();a.set_power(false);
 if a.set_rom_1581(std::slice::from_raw_parts(rom,rom_len)).is_err() {return false;}
 if a.set_board_type(DriveType::Drive1581).is_err() {return false;}
 if a.mount(DiskImage{kind:DiskKind::D81,bytes:std::slice::from_raw_parts(disk,disk_len).to_vec(),backing_path:None,read_only:true}).is_err(){return false;}
 a.set_power(true);
 let mut d=Bridge{a,b:Drive1541::new_position_b(),iec:IecCore::new(),clock:0,last:0,pa:0xff};d.fold();
 DRIVE.with(|s|*s.borrow_mut()=Some(d));true
}
#[no_mangle]
pub extern "C" fn retro_drive_tick(cycles:u32) {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.clock+=cycles as u64;}});}
#[no_mangle]
pub extern "C" fn retro_drive_flush() {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.catch_up(d.clock);d.fold();}});}
#[no_mangle]
pub extern "C" fn retro_drive_read()->u8 {DRIVE.with(|s|{let mut s=s.borrow_mut();let d=s.as_mut().unwrap();d.catch_up(d.clock);d.fold();d.iec.iecbus_callback_read(d.clock)})}
#[no_mangle]
pub extern "C" fn retro_drive_write(pa:u8) {DRIVE.with(|s|{let mut s=s.borrow_mut();let d=s.as_mut().unwrap();d.catch_up(d.clock+1);for (n,e) in d.iec.iecbus_callback_write(!pa,d.clock+1){pair_deliver_atn_edge(&mut d.a,&mut d.b,n,e);}d.pa=pa;});}
#[no_mangle]
pub extern "C" fn retro_drive_peek(address:u16)->u8 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.drive_peek(address))}
#[no_mangle]
pub extern "C" fn retro_drive_pc()->u16 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.cpu().reg_pc)}

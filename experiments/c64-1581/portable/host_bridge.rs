// SPDX-License-Identifier: GPL-3.0-or-later
// Host C ABI test adapter to the extracted no_std 1581 core.
use std::cell::RefCell;
use retro_drive1581::drive1581::Drive1581;
use retro_drive1581::iec::{IecCore,DriveType,AtnEdge};
struct Bridge { a:Drive1581,iec:IecCore,clock:u64,last:u64,pa:u8 }
impl Bridge {
 fn catch_up(&mut self,target:u64) {self.iec.set_slot_type(8,DriveType::Drive1581);
 self.iec.sync_drive_slots(Some(8),None,self.pa);
 if target>self.last {
 let bus=core::array::from_fn(|i|self.iec.drv_bus(i));
 self.a.run_cycles(target-self.last, ((65536u64*1_000_000)/985248) as u32,0,self.iec.drv_port(),self.iec.cpu_bus(),&bus);
 }
 self.last=target;
 self.iec.drive_set_data_no_fold_slot(8,self.a.pb_iec_output());}
 fn fold(&mut self) {self.iec.set_slot_type(8,DriveType::Drive1581);self.iec.sync_drive_slots(Some(8),None,self.pa);self.iec.iec_drive_write_typed(!self.a.pb_iec_output(),0,DriveType::Drive1581);}
}
thread_local! {static DRIVE:RefCell<Option<Bridge>>=const {RefCell::new(None)};}
#[no_mangle]
pub unsafe extern "C" fn retro_drive_init(rom:*const u8,rom_len:usize,disk:*const u8,disk_len:usize)->bool {
 if rom.is_null() || disk.is_null() || rom_len!=32768 || disk_len!=819200 {return false;}
 let mut a=Drive1581::new(0);
 if a.set_rom(std::slice::from_raw_parts(rom,rom_len)).is_err(){return false;}
 a.attach(std::slice::from_raw_parts(disk,disk_len).to_vec(),true);
 a.power_on(0);
 let mut d=Bridge{a,iec:IecCore::new(),clock:0,last:0,pa:0xff};d.fold();
 DRIVE.with(|s|*s.borrow_mut()=Some(d));true
}
#[no_mangle]
pub extern "C" fn retro_drive_tick(cycles:u32) {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.clock+=cycles as u64;}});}
#[no_mangle]
pub extern "C" fn retro_drive_flush() {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.catch_up(d.clock);d.fold();}});}
#[no_mangle]
pub extern "C" fn retro_drive_read()->u8 {DRIVE.with(|s|{let mut s=s.borrow_mut();let d=s.as_mut().unwrap();d.catch_up(d.clock);d.fold();d.iec.iecbus_callback_read(d.clock)})}
#[no_mangle]
pub extern "C" fn retro_drive_write(pa:u8) {DRIVE.with(|s|{let mut s=s.borrow_mut();let d=s.as_mut().unwrap();d.catch_up(d.clock+1);for (n,e) in d.iec.iecbus_callback_write(!pa,d.clock+1){if n==0 && matches!(e,AtnEdge::Cia1581Flag{fire:true}) {d.a.atn_flag();}}d.pa=pa;});}
#[no_mangle]
pub extern "C" fn retro_drive_peek(address:u16)->u8 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.peek(address))}
#[no_mangle]
pub extern "C" fn retro_drive_pc()->u16 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.core.reg_pc)}

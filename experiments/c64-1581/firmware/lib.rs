// SPDX-License-Identifier: GPL-3.0-or-later
// Firmware C ABI adapter; all entry points run on emulation core 0.
#![no_std]
extern crate alloc;
use core::cell::{RefCell, UnsafeCell};
use core::alloc::{GlobalAlloc, Layout};
extern "C" { fn psram_malloc(n:usize)->*mut u8; fn psram_free(p:*mut u8); fn retro_drive_panic()->!; fn retro_drive_time_us()->u32; }
// One dedicated drive RAM allocation. All other allocations remain in PSRAM.
#[repr(align(8))] struct FastRam(UnsafeCell<[u8;8192]>);
unsafe impl Sync for FastRam {}
static FAST_RAM:FastRam=FastRam(UnsafeCell::new([0;8192]));
static mut FAST_RAM_USED:bool=false;
#[no_mangle] pub static mut retro_drive_enabled:bool=false;
#[no_mangle] pub static mut retro_drive_pending:u32=0;
struct Psram;
unsafe impl GlobalAlloc for Psram {
 unsafe fn alloc(&self, layout:Layout)->*mut u8 {
  if layout.size()==8192 && layout.align()<=8 && !FAST_RAM_USED {
   FAST_RAM_USED=true;return FAST_RAM.0.get() as *mut u8;
  }
  let align=layout.align().max(core::mem::size_of::<usize>());
  let Some(n)=layout.size().checked_add(align).and_then(|n|n.checked_add(core::mem::size_of::<usize>())) else{return core::ptr::null_mut()};
  let raw=psram_malloc(n); if raw.is_null(){return raw;}
  let aligned=((raw as usize+core::mem::size_of::<usize>()+align-1)&!(align-1)) as *mut u8;
  (aligned as *mut *mut u8).sub(1).write(raw); aligned
 }
 unsafe fn dealloc(&self,p:*mut u8,_:Layout){if p==FAST_RAM.0.get() as *mut u8 {FAST_RAM_USED=false;return;}psram_free((p as *mut *mut u8).sub(1).read());}
}
#[global_allocator] static ALLOC:Psram=Psram;
#[panic_handler] fn panic(_: &core::panic::PanicInfo)->! {unsafe{retro_drive_panic()}}
// C ABI contract: emulation core only, never called by IRQs, core 1, or concurrently.
struct SingleCore(UnsafeCell<RefCell<Option<Bridge>>>);
unsafe impl Sync for SingleCore {}
impl SingleCore {fn with<R>(&self,f:impl FnOnce(&RefCell<Option<Bridge>>)->R)->R {unsafe{f(&*self.0.get())}}}

use retro_drive1581::drive1581::Drive1581;
use retro_drive1581::iec::{IecCore,DriveType,AtnEdge};
struct Bridge { a:Drive1581,iec:IecCore,clock:u64,last:u64,pa:u8,run_us:u64,run_calls:u64,empty_calls:u64,empty_us:u64 }
impl Bridge {
 fn collect(&mut self) {unsafe{self.clock+=retro_drive_pending as u64;retro_drive_pending=0;}}
 fn configure_bus(&mut self) {
 self.iec.set_slot_type(8,DriveType::Drive1581);
 self.iec.sync_drive_slots(Some(8),None,self.pa);
 }
 fn catch_up(&mut self,target:u64) {
 if target>self.last {
 // This firmware exposes one drive at slot 8. All other slots release
 // the bus; run_cycles explicitly ignores its own slot's array entry.
 const BUS:[u8;16]=[0xff;16];
 let before=self.a.core.instructions;
 let started=unsafe{retro_drive_time_us()};
 #[cfg(feature="drive-profile")] retro_drive1581::profile::begin();
 self.a.run_cycles(target-self.last, ((65536u64*1_000_000)/985248) as u32,0,self.iec.drv_port(),self.iec.cpu_bus(),&BUS);
 #[cfg(feature="drive-profile")] retro_drive1581::profile::end();
 let elapsed=unsafe{retro_drive_time_us()}.wrapping_sub(started) as u64;
 self.run_us+=elapsed;
 if self.a.core.instructions==before {self.empty_calls+=1;self.empty_us+=elapsed;}
 self.run_calls+=1;
 }
 self.last=target;
 self.iec.drive_set_data_no_fold_slot(8,self.a.pb_iec_output());}
 fn fold(&mut self) {self.iec.iec_drive_write_typed(!self.a.pb_iec_output(),0,DriveType::Drive1581);}
}
static DRIVE:SingleCore=SingleCore(UnsafeCell::new(RefCell::new(None)));
#[no_mangle]
pub unsafe extern "C" fn retro_drive_init(rom:*const u8,rom_len:usize,disk:*const u8,disk_len:usize)->bool {
 if rom.is_null() || disk.is_null() || rom_len!=32768 || disk_len!=819200 {return false;}
 retro_drive_close();
 let mut a=Drive1581::new(0);
 // Caller supplies the permanent embedded flash ROM, valid through close/reset.
 a.set_rom_flash(&*(rom as *const [u8;32768]));
 a.attach(core::slice::from_raw_parts(disk,disk_len).to_vec(),true);
 a.power_on(0);
 let mut d=Bridge{a,iec:IecCore::new(),clock:0,last:0,pa:0xff,run_us:0,run_calls:0,empty_calls:0,empty_us:0};d.configure_bus();d.fold();
 DRIVE.with(|s|*s.borrow_mut()=Some(d));retro_drive_pending=0;retro_drive_enabled=true;true
}
#[no_mangle]
pub extern "C" fn retro_drive_tick(cycles:u32) {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){unsafe{retro_drive_pending=retro_drive_pending.wrapping_add(cycles);}}});}
#[no_mangle]
pub extern "C" fn retro_drive_flush() {DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.collect();d.catch_up(d.clock);d.fold();}});}
#[no_mangle]
pub extern "C" fn retro_drive_read()->u8 {DRIVE.with(|s|{let mut s=s.borrow_mut();let Some(d)=s.as_mut() else{return 0xff};d.collect();d.catch_up(d.clock);d.fold();d.iec.iecbus_callback_read(d.clock)})}
#[no_mangle]
pub extern "C" fn retro_drive_write(pa:u8) {DRIVE.with(|s|{let mut s=s.borrow_mut();let Some(d)=s.as_mut() else{return};d.collect();d.catch_up(d.clock+1);for (n,e) in d.iec.iecbus_callback_write(!pa,d.clock+1){if n==0 && matches!(e,AtnEdge::Cia1581Flag{fire:true}) {d.a.atn_flag();}}d.pa=pa;});}
#[no_mangle]
pub extern "C" fn retro_drive_peek(address:u16)->u8 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.peek(address))}
#[no_mangle]
pub extern "C" fn retro_drive_pc()->u16 {DRIVE.with(|s|s.borrow().as_ref().unwrap().a.core.reg_pc)}

#[no_mangle] pub extern "C" fn retro_drive_active()->bool {DRIVE.with(|s|s.borrow().is_some())}
#[no_mangle] pub extern "C" fn retro_drive_close(){
#[cfg(feature="drive-profile")] retro_drive1581::profile::reset();
unsafe{retro_drive_enabled=false;retro_drive_pending=0;}DRIVE.with(|s|*s.borrow_mut()=None);}
#[no_mangle] pub extern "C" fn retro_drive_reset(){
#[cfg(feature="drive-profile")] retro_drive1581::profile::reset();
unsafe{retro_drive_pending=0;}DRIVE.with(|s|{if let Some(d)=s.borrow_mut().as_mut(){d.a.reset(0);d.clock=0;d.last=0;d.run_us=0;d.run_calls=0;d.empty_calls=0;d.empty_us=0;d.pa=0xff;d.iec=IecCore::new();d.configure_bus();d.fold();}});}

// Cumulative counters since mount/reset; consumers take deltas between samples.
#[no_mangle] pub extern "C" fn retro_drive_run_us()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.run_us))}
#[no_mangle] pub extern "C" fn retro_drive_run_calls()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.run_calls))}

#[no_mangle] pub extern "C" fn retro_drive_instructions()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.a.core.instructions))}

#[no_mangle] pub extern "C" fn retro_drive_empty_calls()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.empty_calls))}

#[no_mangle] pub extern "C" fn retro_drive_empty_us()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.empty_us))}

#[no_mangle] pub extern "C" fn retro_drive_idle_loops()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.a.core.idle_loops))}

#[no_mangle] pub extern "C" fn retro_drive_idle_tail_instructions()->u64 {DRIVE.with(|s|s.borrow().as_ref().map_or(0,|d|d.a.core.idle_tail_instructions))}

#[no_mangle] pub extern "C" fn retro_drive_profile(index:u32)->u64 {
 #[cfg(feature="drive-profile")] { return retro_drive1581::profile::get(index); }
 #[cfg(not(feature="drive-profile"))] { let _=index; 0 }
}

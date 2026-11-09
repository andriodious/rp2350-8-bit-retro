// Single emulation-core diagnostics only. Never used by IRQ/core 1.
// Sparse pseudorandom sampling avoids locking the sample to an IEC cadence.
extern "C" { fn retro_drive_time_us() -> u32; }
static mut SEED: u32 = 0x1581;
static mut ACTIVE: bool = false;
static mut LAST: u32 = 0;
static mut STAGE: usize = 0;
static mut CALLS: u64 = 0;
static mut SAMPLES: u64 = 0;
static mut TIMES: [u64; 4] = [0; 4];
pub fn reset() { unsafe { SEED=0x1581; ACTIVE=false; CALLS=0; SAMPLES=0; TIMES=[0;4]; } }
pub fn begin() { unsafe {
    CALLS+=1; SEED=SEED.wrapping_mul(1664525).wrapping_add(1013904223);
    ACTIVE=SEED >> 24 == 0;
    if ACTIVE { SAMPLES+=1; STAGE=0; LAST=retro_drive_time_us(); }
} }
#[inline]
pub fn mark(stage: usize) { unsafe {
    if ACTIVE { let now=retro_drive_time_us(); TIMES[STAGE]+=now.wrapping_sub(LAST) as u64; LAST=now; STAGE=stage; }
} }
pub fn end() { mark(0); unsafe { ACTIVE=false; } }
pub fn get(index: u32) -> u64 { unsafe { match index {
    0=>CALLS, 1=>SAMPLES, 2..=5=>TIMES[(index-2) as usize], _=>0
} } }

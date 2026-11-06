#!/usr/bin/env python3
"""Generate a drive-only no_std crate from the pinned, unmodified TRX64 checkout.

Generated sources retain their upstream copyright notices. Snapshot support is
excluded; this crate is an embedded compilation experiment, not firmware yet.
"""
import argparse
from pathlib import Path
import subprocess

PIN = 'a86341c8b2add9841dab1a8340fb76d71121e1e3'
p = argparse.ArgumentParser()
p.add_argument('--trx64', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert subprocess.check_output(['git', '-C', str(a.trx64), 'rev-parse', 'HEAD'], text=True).strip() == PIN
subprocess.run(['git', '-C', str(a.trx64), 'diff', '--exit-code', 'HEAD'], check=True)
a.output.mkdir(parents=True, exist_ok=False)
src = a.output / 'src'
src.mkdir()
modules = ['drive1581', 'drive_6510core', 'ciacore', 'wd177x', 'fdd', 'iec', 'fdc_controller']
for name in modules:
    text = (a.trx64 / 'crates/trx64-core/src' / (name + '.rs')).read_text()
    # Keep source declarations, but omit desktop snapshot entry points entirely.
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if ('fn snapshot_' in line or 'use crate::vice_snapshot_stream' in line or 'fn checkpoint(' in line or 'fn restore(' in line):
            lines[i] = '#[cfg(feature="snapshots")]\n' + line
    text = ''.join(lines)
    # Add alloc imports after module documentation, before first non-comment item.
    lines = text.splitlines(keepends=True)
    index = next(i for i,l in enumerate(lines) if l.strip() and not l.lstrip().startswith(('//', '#![')))
    lines.insert(index, '#[allow(unused_imports)]\nuse alloc::{boxed::Box, vec::Vec, string::{String, ToString}, format, vec};\n')
    text = ''.join(lines).replace(', serde::Serialize, serde::Deserialize', '')
    if name == 'drive1581':
        text = text.replace('Box::new([0u8; 0x2000])', 'vec![0u8; 0x2000].into_boxed_slice().try_into().unwrap()')
        text = text.replace('Box::new([0u8; 0x8000])', 'vec![0u8; 0x8000].into_boxed_slice().try_into().unwrap()')
        text = text.replace('let mut rom = vec![0u8; 0x8000].into_boxed_slice()', 'let mut rom: Box<[u8; 0x8000]> = vec![0u8; 0x8000].into_boxed_slice()')
        text = text.replace('self.fast_log.pb5_changes.push((self.clk, dir));', '// Embedded build: do not accumulate an unbounded diagnostic trace.')
        text = text.replace('pub struct Drive1581 {', "// ROM remains read-only; firmware may borrow its permanent flash image.\nenum Rom1581 { Owned(Box<[u8; 0x8000]>), Flash(&'static [u8; 0x8000]) }\nimpl core::ops::Deref for Rom1581 {\n    type Target=[u8; 0x8000];\n    fn deref(&self)->&Self::Target { match self { Self::Owned(b)=>b, Self::Flash(b)=>b } }\n}\npub struct Drive1581 {")
        text = text.replace('rom: Box<[u8; 0x8000]>,', 'rom: Rom1581,')
        text = text.replace('rom_next: Option<Box<[u8; 0x8000]>>,', 'rom_next: Option<Rom1581>,')
        text = text.replace('rom: vec![0u8; 0x8000].into_boxed_slice().try_into().unwrap(),', 'rom: Rom1581::Owned(vec![0u8; 0x8000].into_boxed_slice().try_into().unwrap()),')
        text = text.replace('self.rom_next = Some(rom);', 'self.rom_next = Some(Rom1581::Owned(rom));')
        text = text.replace('    /// The ROM in force.', "    pub fn set_rom_flash(&mut self, rom: &'static [u8; 0x8000]) { self.rom_next=Some(Rom1581::Flash(rom)); }\n\n    /// The ROM in force.")
        # Keep plain RAM/ROM accesses inline without pulling the large CIA/FDC
        # implementations into every opcode. The original mapped implementations
        # remain the slow path, preserving clocks, side effects and open-bus data.
        marker = "impl<F: BoardFdc> DriveCore6510Bus for Bus1581<'_, F> {"
        start = text.index(marker)
        end = text.index('    // No rotating GCR surface', start)
        mapped = text[start + len(marker):end]
        assert mapped.count('fn read(') == 1 and mapped.count('fn write(') == 1
        mapped = mapped.replace('#[inline]', '#[inline(never)]')
        mapped = mapped.replace('fn read(', 'fn read_mapped(').replace('fn write(', 'fn write_mapped(')
        fast = """
    #[inline(always)]
    fn read(&mut self, addr: u16) -> u8 {
        let v = if addr < 0x2000 {
            self.ram[addr as usize]
        } else if addr >= 0x8000 {
            self.rom[(addr & 0x7fff) as usize]
        } else if addr < 0x4000 {
            return *self.cpu_last_data;
        } else {
            return self.read_mapped(addr);
        };
        *self.cpu_last_data = v;
        v
    }
    #[inline(always)]
    fn write(&mut self, addr: u16, val: u8) {
        if addr < 0x2000 {
            *self.cpu_last_data = val;
            self.ram[addr as usize] = val;
        } else if (0x4000..0x8000).contains(&addr) {
            self.write_mapped(addr, val);
        } else {
            *self.cpu_last_data = val;
        }
    }

"""
        text = text[:start] + "impl<F: BoardFdc> Bus1581<'_, F> {" + mapped + '}\n\n' + marker + fast + text[end:]
        # Only the built-in controller has a constant PB6 during a CPU slice.
        text = text.replace('pub(crate) trait BoardFdc {', 'pub(crate) trait BoardFdc {\n    const IDLE_PB_SAFE: bool = false;')
        text = text.replace("impl BoardFdc for BuiltIn<'_> {", "impl BoardFdc for BuiltIn<'_> {\n    const IDLE_PB_SAFE: bool = true;")
        text = text.replace('fn run_slice<F: BoardFdc>(', 'fn run_slice_impl<F: BoardFdc, const OPT: bool>(')
        text = text.replace('        drive_6510core_execute(core, &mut bus, int);',
                            '        if OPT && matches!(core.reg_pc, 0x06e4 | 0x06e7) && skip_data_wait(core, int, stop_clk, &mut bus) { continue; }\n        drive_6510core_execute(core, &mut bus, int);')
        text += (Path(__file__).with_name('idle_wait.rs')).read_text()
        text += '\n#[cfg(feature="idle-validation")]\n' + (Path(__file__).with_name('idle_validation.rs')).read_text()
    if name == 'drive_6510core':
        text = text.replace('pub struct DriveCore6510 {', 'pub struct DriveCore6510 {\n    pub instructions: u64,\n    pub idle_loops: u64,\n    pub idle_tail_instructions: u64,')
        text = text.replace('        DriveCore6510 {', '        DriveCore6510 {\n            instructions: 0,\n            idle_loops: 0,\n            idle_tail_instructions: 0,')
        text = text.replace('    let reg_pc0 = ex.core.reg_pc;', '    ex.core.instructions = ex.core.instructions.wrapping_add(1);\n    let reg_pc0 = ex.core.reg_pc;')
    if name == 'ciacore':
        text = text.replace('#[derive(Clone)]', '#[derive(Clone, Debug, PartialEq, Eq)]')
        start = text.index('    static TABLE: std::sync::OnceLock')
        end = text.index('\n}\n', start)
        text = text[:start] + '    static TABLE: [u16; CIAT_TABLEN] = build_table();\n    &TABLE' + text[end:]
        text = text.replace('fn build_table() -> Box<[u16; CIAT_TABLEN]> {', 'const fn build_table() -> [u16; CIAT_TABLEN] {')
        text = text.replace('let mut t = Box::new([0u16; CIAT_TABLEN]);\n    for (i, slot) in t.iter_mut().enumerate() {\n        let i = i as u16;', 'let mut t = [0u16; CIAT_TABLEN];\n    let mut index = 0;\n    while index < CIAT_TABLEN {\n        let i = index as u16;')
        text = text.replace('*slot = tmp;', 't[index] = tmp;\n        index += 1;')
    if name == 'fdd':
        start = text.index('    static T: std::sync::OnceLock')
        end = text.index('\n}\n', start)
        text = text[:start] + '''    const fn make() -> [u16; 256] {
        let mut t = [0u16; 256];
        let mut i = 0;
        while i < 256 {
            let mut w = (i as u32) << 8;
            let mut n = 0;
            while n < 8 {
                w = if w & 0x8000 != 0 { (w << 1) ^ 0x1021 } else { w << 1 };
                n += 1;
            }
            t[i] = w as u16;
            i += 1;
        }
        t
    }
    static T: [u16; 256] = make();
    &T''' + text[end:]
    if name == 'iec':
        # Single emulation-core instance only. Never call from an IRQ or core 1.
        text = text.replace('use std::cell::Cell;', 'use core::cell::Cell;')
        start = text.index('thread_local! {')
        end = text.index('// A zero-sized facade', start)
        text = text[:start] + '''// Safety contract: one instance, accessed only by the emulation core.
struct SingleCoreStatus(IecbusStatusArrays);
static STATUS_LOCK: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);
unsafe impl Sync for SingleCoreStatus {}
static IECBUS_STATUS_ARRAYS_TLS: SingleCoreStatus = SingleCoreStatus(IecbusStatusArrays {
    truedrive: [const { Cell::new(0) }; IECBUS_NUM],
    drivetype: [const { Cell::new(0) }; IECBUS_NUM],
    iecdevice: [const { Cell::new(0) }; IECBUS_NUM],
    virtualdevices: [const { Cell::new(0) }; IECBUS_NUM],
});
impl SingleCoreStatus {
    fn with<R>(&self, f: impl FnOnce(&IecbusStatusArrays) -> R) -> R {
        use core::sync::atomic::Ordering;
        while STATUS_LOCK.compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed).is_err() { core::hint::spin_loop(); }
        struct Unlock;
        impl Drop for Unlock { fn drop(&mut self) { STATUS_LOCK.store(false, Ordering::Release); } }
        let _unlock = Unlock;
        f(&self.0)
    }
}

''' + text[end:]
    (src / (name + '.rs')).write_text(text)
(src / 'lib.rs').write_text('''#![no_std]
extern crate alloc;
pub mod expansion {
    pub trait AsAny { fn as_any_mut(&mut self) -> &mut dyn core::any::Any; }
    impl<T: core::any::Any> AsAny for T {
        fn as_any_mut(&mut self) -> &mut dyn core::any::Any { self }
    }
}
''' + ''.join(f'pub mod {m};\n' for m in modules))
(a.output / 'Cargo.toml').write_text('''[package]
name="retro-drive1581"
version="0.0.1"
edition="2021"
[features]
snapshots=[]
idle-validation=[]
idle-reference=[]
idle-sram=[]
[profile.release]
panic="abort"
opt-level=2
''')
print(a.output)

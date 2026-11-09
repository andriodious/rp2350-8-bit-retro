from pathlib import Path
import sys
p=Path(sys.argv[1]);variant=sys.argv[2];s=p.read_text()
if variant in ('prealarm','combined'):
 start=s.index('fn run_slice_impl');end=s.index('/// The 1581 board.',start)
 body=s[start:end]
 old='        #[cfg(feature="drive-profile")] crate::profile::mark(2);\n        if OPT && matches!(core.reg_pc, 0x06e4 | 0x06e7) && skip_data_wait(core, int, stop_clk, &mut bus) { #[cfg(feature="drive-profile")] crate::profile::mark(0); continue; }\n'
 assert body.count(old)==1
 body=body.replace(old,'')
 body=body.replace('        *reset_pending = false;',old+'        *reset_pending = false;')
 # Explicitly exclude pending reset before attempting shortcut.
 body=body.replace('if OPT && matches!', 'if OPT && !*reset_pending && matches!')
 s=s[:start]+body+s[end:]
if variant in ('straight','combined'):
 start=s.index('    // Inputs and uploaded code');end=s.index('\n}\n\nfn run_slice<',start)
 old=s[start:end]
 # Derive the same operations, but only one pass: initial BEQ, pairs, BIT, BEQ.
 branch=old[old.index('        core.clk += 3;'):old.index('        progressed = true;')]
 bit=old[old.index('        bus.cia.clk = core.clk + 3;'):old.index('        progressed = true;',old.index('        bus.cia.clk = core.clk + 3;'))]
 pairs=old[old.index('    core.clk += loops * 7;'):old.index('    progressed = true;',old.index('    core.clk += loops * 7;'))]
 new='''    let alarm = bus.cia.next_alarm_clk();
    let mut progressed = false;
    if core.reg_pc == 0x06e7 {
        if alarm.saturating_sub(core.clk) <= 3 { return false; }
'''+branch+'''        progressed = true;
        if core.clk >= stop { return true; }
    }
    let end = stop.min(alarm.saturating_sub(1));
    let loops = end.saturating_sub(core.clk) / 7;
    if loops != 0 {
'''+pairs+'''        progressed = true;
    }
    if core.clk >= stop || alarm.saturating_sub(core.clk) <= 4 { return progressed; }
'''+bit+'''    if core.clk >= stop || alarm.saturating_sub(core.clk) <= 3 { return true; }
'''+branch+'''    true'''
 s=s[:start]+new+s[end:]
p.write_text(s)

// DOS job-queue scan: batch only complete, taken B117..B126 iterations.
// All accesses are RAM/ROM, with no peripheral side effects. Leave exits and
// partial iterations to the interpreter, and never cross a CIA/IEC deadline.
#[inline(never)]
fn skip_job_scan<F: BoardFdc>(core: &mut DriveCore6510, int: &mut IntStatus,
    stop: u64, bus: &mut Bus1581<'_, F>) -> bool {
    if core.reg_pc != 0xb117 || core.is_jammed || core.d_bank_base.is_some()
        || core.d_bank_start != 0 || core.d_bank_limit != 0
        || int.global_pending_int != 0 || int.nirq != 0 || int.nnmi != 0
        || !bus.cia.irq_events.is_empty()
        || bus.rom[0x3117..0x3129] != [0xb5,0xa8,0xc9,0xff,0xd0,0x14,
            0x88,0x30,0x05,0xb9,0x02,0x00,0x30,0x0c,0xca,0x10,0xef,0xa5]
    { return false; }
    let end = stop.min(bus.cia.next_alarm_clk().saturating_sub(1));
    let mut progressed = false;
    while core.reg_x > 0 && core.reg_x <= 0x80 {
        if bus.ram[0xa8u8.wrapping_add(core.reg_x) as usize] != 0xff { break; }
        let y = core.reg_y.wrapping_sub(1);
        let (a, cycles, instructions) = if y & 0x80 != 0 { (0xff,18,7) }
            else { (bus.ram[2 + y as usize],23,9) };
        if a & 0x80 != 0 && y & 0x80 == 0 { break; }
        if end.saturating_sub(core.clk) < cycles { break; }
        core.reg_a = a;
        core.reg_y = y;
        core.reg_x -= 1;
        core.reg_p |= 1; // CMP #$ff on an equal byte sets carry.
        core.flag_n = core.reg_x;
        core.flag_z = core.reg_x;
        core.clk += cycles;
        core.instructions = core.instructions.wrapping_add(instructions);
        core.last_opcode_addr = 0xb126;
        core.last_opcode_info = 0x110;
        int.last_opcode_info_ptr = 0x110;
        *bus.cpu_last_data = 0xa5; // Taken BPL dummy-read at B128.
        progressed = true;
    }
    progressed
}

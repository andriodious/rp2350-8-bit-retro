
// Presto adaptation: batch complete iterations of the uploaded loader's
// BIT $4001 / BEQ $06E4 DATA wait. Never bypass an alarm or a caller's IEC
// synchronisation boundary. Unknown code, timer-output modes and interrupts
// always use the original interpreter. The drive remains fully clocked.
#[cfg_attr(feature="idle-sram", inline(never))]
#[cfg_attr(all(feature="idle-sram", target_arch="arm"), link_section=".time_critical.retro_1581_idle_wait")]
fn skip_data_wait<F: BoardFdc>(
    core: &mut DriveCore6510, int: &mut IntStatus, stop: u64, bus: &mut Bus1581<'_, F>,
) -> bool {
    if !matches!(core.reg_pc, 0x06e4 | 0x06e7) || !F::IDLE_PB_SAFE || core.is_jammed
        || core.clk >= stop
        || core.reg_a != 1 || core.d_bank_base.is_some()
        || (core.reg_pc == 0x06e4 && (core.last_opcode_addr != 0x06e7 || core.last_opcode_info != 0x1f0))
        || (core.reg_pc == 0x06e7 && (core.last_opcode_addr != 0x06e4 || core.last_opcode_info != 0x2c))
        || int.last_opcode_info_ptr != core.last_opcode_info
        || int.global_pending_int != 0 || int.nirq != 0 || int.nnmi != 0
        || !bus.cia.irq_events.is_empty()
        || (bus.cia.c_cia[14] | bus.cia.c_cia[15]) & 2 != 0
        || bus.ram[0x06e1..0x06ea] != [0x58, 0xa9, 1, 0x2c, 1, 0x40, 0xf0, 0xfb, 0x78]
    { return false; }
    // Same expression as Ports::read_pb; PB6 is constant for BuiltIn. PC pulse
    // is a no-op on this board and timer-driven PB6/7 was excluded above.
    let value = (((bus.cia.c_cia[1] & 0x1a) | bus.ports.iecbus.drv_port) ^ 0x85)
        | bus.ports.fdc.pb6();
    if value & 1 != 0 || bus.cia.last_read != value || core.flag_z != 0
        || core.flag_n != value & 0x80 || core.reg_p & 0x40 != value & 0x40
    { return false; }
    // Inputs and uploaded code cannot change while executing only this
    // read-only waiting loop. Reuse validation until the slice/alarm boundary,
    // rather than returning through the dispatcher after each tail instruction.
    let mut progressed = false;
    loop {
        if core.clk >= stop { return progressed; }
    // Finish a taken branch without entering the general opcode dispatcher.
    // Like the interpreter, one instruction can finish beyond the slice target.
    // Do not cross any alarm even when it would only be serviced next time.
    if core.reg_pc == 0x06e7 {
        if bus.cia.next_alarm_clk().saturating_sub(core.clk) <= 3 { return progressed; }
        core.clk += 3;
        core.reg_pc = 0x06e4;
        core.d_bank_start = 0;
        core.d_bank_limit = 0;
        core.last_opcode_addr = 0x06e7;
        core.last_opcode_info = 0x1f0;
        int.last_opcode_info_ptr = 0x1f0;
        core.instructions = core.instructions.wrapping_add(1);
        core.idle_tail_instructions = core.idle_tail_instructions.wrapping_add(1);
        *bus.cpu_last_data = 0x78;
        progressed = true;
        continue;
    }
    let end = stop.min(bus.cia.next_alarm_clk().saturating_sub(1));
    let loops = end.saturating_sub(core.clk) / 7;
    if loops == 0 {
        if bus.cia.next_alarm_clk().saturating_sub(core.clk) <= 4 { return progressed; }
        // BIT absolute: operand read at +3, instruction boundary at +4.
        // Guards above established that N/V/Z already equal this input.
        bus.cia.clk = core.clk + 3;
        bus.cia.read_clk = core.clk + 3;
        bus.cia.read_offset = 0;
        bus.ports.clk = core.clk + 3;
        core.clk += 4;
        core.reg_pc = 0x06e7;
        core.last_opcode_addr = 0x06e4;
        core.last_opcode_info = 0x2c;
        int.last_opcode_info_ptr = 0x2c;
        core.instructions = core.instructions.wrapping_add(1);
        core.idle_tail_instructions = core.idle_tail_instructions.wrapping_add(1);
        *bus.cpu_last_data = value;
        progressed = true;
        continue;
    }
    core.clk += loops * 7;
    core.instructions = core.instructions.wrapping_add(loops * 2);
    core.idle_loops = core.idle_loops.wrapping_add(loops);
    // The last BIT read is four cycles before the final taken branch ends.
    bus.cia.clk = core.clk - 4;
    bus.cia.read_clk = core.clk - 4;
    bus.cia.read_offset = 0;
    bus.ports.clk = core.clk - 4;
    *bus.cpu_last_data = 0x78; // BEQ's dummy read of the following SEI.
    progressed = true;
    }
}

fn run_slice<F: BoardFdc>(core: &mut DriveCore6510, int: &mut IntStatus,
    reset: &mut bool, stop: u64, bus: Bus1581<'_, F>) {
    run_slice_impl::<F, { !cfg!(feature="idle-reference") }>(core, int, reset, stop, bus);
}

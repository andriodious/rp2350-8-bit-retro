// Guarded CLOCK-edge waits in the uploaded D81 serial send/receive routines.
// Do not execute the following port write/read or cross a caller/IRQ deadline.
// The initial BIT is always interpreted so its complete CIA side effects and
// flags are established before the shortcut is eligible.
#[cfg_attr(feature="idle-sram", inline(never))]
#[cfg_attr(all(feature="idle-sram", target_arch="arm"), link_section=".time_critical.retro_1581_serial_wait")]
fn skip_serial_wait<F: BoardFdc>(
    core: &mut DriveCore6510, int: &mut IntStatus, stop: u64, bus: &mut Bus1581<'_, F>,
) -> bool {
    let (bit_pc, branch_op, dummy) = match core.reg_pc {
        0x064b | 0x064e => (0x064b, 0xf0, 0x8c),
        0x065d | 0x0660 => (0x065d, 0xd0, 0xca),
        0x0676 | 0x0679 => (0x0676, 0xf0, 0xad),
        0x0683 | 0x0686 => (0x0683, 0xd0, 0xad),
        _ => return false,
    };
    let branch_pc = bit_pc + 3;
    let branch_info = 0x100 | branch_op as u32;
    // A held IRQ is inert while I is set. The guarded preceding BIT/branch
    // cannot carry SEI/CLI delay flags, and IRQPEND without a held IRQ, NMI and reset still fall back.
    // The interpreter's do_interrupt performs no state changes in this case.
    let pending = int.global_pending_int;
    let masked_irq = core.reg_p & 4 != 0 && pending & IK_IRQ != 0
        && pending & !(IK_IRQ | crate::drive_6510core::IK_IRQPEND) == 0;
    if !F::IDLE_PB_SAFE || core.is_jammed
        || core.clk >= stop
        || core.reg_a != 4 || core.d_bank_base.is_some()
        || core.d_bank_start != 0 || core.d_bank_limit != 0
        || (core.reg_pc == bit_pc && (core.last_opcode_addr != branch_pc || core.last_opcode_info != branch_info))
        || (core.reg_pc == branch_pc && (core.last_opcode_addr != bit_pc || core.last_opcode_info != 0x2c))
        || int.last_opcode_info_ptr != core.last_opcode_info
        || ((int.global_pending_int != 0 || int.nirq != 0) && !masked_irq) || int.nnmi != 0
        || !bus.cia.irq_events.is_empty()
        || (bus.cia.c_cia[14] | bus.cia.c_cia[15]) & 2 != 0
        || bus.ram[(bit_pc - 2) as usize..(bit_pc + 6) as usize]
            != [0xa9, 4, 0x2c, 1, 0x40, branch_op, 0xfb, dummy]
    { return false; }
    // Same expression as Ports::read_pb; PB6 is constant for BuiltIn. PC pulse
    // is a no-op on this board and timer-driven PB6/7 was excluded above.
    let value = (((bus.cia.c_cia[1] & 0x1a) | bus.ports.iecbus.drv_port) ^ 0x85)
        | bus.ports.fdc.pb6();
    if ((value & 4 == 0) != (branch_op == 0xf0))
        || bus.cia.last_read != value || core.flag_z != u8::from(value & 4 != 0)
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
    if core.reg_pc == branch_pc {
        if bus.cia.next_alarm_clk().saturating_sub(core.clk) <= 3 { return progressed; }
        core.clk += 3;
        core.reg_pc = bit_pc;
        core.d_bank_start = 0;
        core.d_bank_limit = 0;
        core.last_opcode_addr = branch_pc;
        core.last_opcode_info = branch_info;
        int.last_opcode_info_ptr = branch_info;
        core.instructions = core.instructions.wrapping_add(1);
        core.serial_tail_instructions = core.serial_tail_instructions.wrapping_add(1);
        *bus.cpu_last_data = dummy;
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
        core.reg_pc = branch_pc;
        core.last_opcode_addr = bit_pc;
        core.last_opcode_info = 0x2c;
        int.last_opcode_info_ptr = 0x2c;
        core.instructions = core.instructions.wrapping_add(1);
        core.serial_tail_instructions = core.serial_tail_instructions.wrapping_add(1);
        *bus.cpu_last_data = value;
        progressed = true;
        continue;
    }
    core.clk += loops * 7;
    core.instructions = core.instructions.wrapping_add(loops * 2);
    core.serial_loops = core.serial_loops.wrapping_add(loops);
    // The last BIT read is four cycles before the final taken branch ends.
    bus.cia.clk = core.clk - 4;
    bus.cia.read_clk = core.clk - 4;
    bus.cia.read_offset = 0;
    bus.ports.clk = core.clk - 4;
    *bus.cpu_last_data = dummy; // Taken branch dummy-read of the following instruction.
    progressed = true;
    }
}

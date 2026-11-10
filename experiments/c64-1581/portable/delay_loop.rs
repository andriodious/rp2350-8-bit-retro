// The ROM's short DEX/BNE delay at AD37. Batch only taken iterations;
// leave X=1 and the final not-taken branch to the normal interpreter.
#[inline(never)]
fn skip_rom_delay<F: BoardFdc>(core: &mut DriveCore6510, int: &mut IntStatus,
    stop: u64, bus: &mut Bus1581<'_, F>) -> bool {
    if core.reg_pc != 0xad37 || core.reg_x < 2 || stop.saturating_sub(core.clk) < 5 || core.is_jammed
        || core.d_bank_base.is_some() || core.d_bank_start != 0 || core.d_bank_limit != 0
        || int.global_pending_int != 0 || int.nirq != 0 || int.nnmi != 0
        || !bus.cia.irq_events.is_empty()
        || bus.rom[0x2d37..0x2d3b] != [0xca,0xd0,0xfd,0xaa]
    { return false; }
    let end = stop.min(bus.cia.next_alarm_clk().saturating_sub(1));
    // At most 254 iterations; bound before converting to 32 bits/dividing.
    let cycles = end.saturating_sub(core.clk).min(1270) as u32;
    let loops = (cycles / 5).min(core.reg_x as u32 - 1);
    if loops == 0 { return false; }
    core.reg_x -= loops as u8;
    core.flag_n = core.reg_x;
    core.flag_z = core.reg_x;
    core.clk += (loops * 5) as u64;
    core.instructions = core.instructions.wrapping_add((loops * 2) as u64);
    core.last_opcode_addr = 0xad38;
    core.last_opcode_info = 0x1d0;
    int.last_opcode_info_ptr = 0x1d0;
    *bus.cpu_last_data = 0xaa; // Taken BNE dummy-read at AD3A.
    true
}

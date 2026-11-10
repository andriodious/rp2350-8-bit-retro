pub mod idle_validation {
    use super::*;

    fn run<const OPT: bool>(d: &mut Drive1581, stop: u64) {
        let clk_ptr = &mut d.core.clk as *mut u64;
        let bus = Bus1581 {
            ram: &mut d.ram, rom: &d.rom, cia: &mut d.cia,
            ports: Ports { fdc: BuiltIn { wd: &mut d.wd, read_only: true },
                iecbus: &mut d.iecbus, number: 0, fast_dir: &mut d.fast_dir,
                fast_log: &mut d.fast_log, led: &mut d.led, clk: 0 },
            clk_ptr, cpu_last_data: &mut d.cpu_last_data,
        };
        #[cfg(feature="drive-profile")] crate::profile::begin();
        run_slice_impl::<_, OPT>(&mut d.core, &mut d.int, &mut d.reset_pending, stop, bus);
        #[cfg(feature="drive-profile")] crate::profile::end();
    }
    fn fixture() -> Drive1581 {
        let mut d = Drive1581::new(8);
        d.reset_pending = false;
        d.int = IntStatus::new();
        d.core = DriveCore6510::new();
        d.core.clk = 100;
        d.core.reg_pc = 0x06e2;
        d.iecbus.drv_port = 0x85; // DATA low in the CPU-facing CIA read.
        d.ram[0x06e1..0x06ef].copy_from_slice(&[
            0x58,0xa9,1,0x2c,1,0x40,0xf0,0xfb,0x78,0xea,0x4c,0xe9,0x06,0xea]);
        d
    }
    fn equal(a: &Drive1581, b: &Drive1581) {
        let mut c = a.core.clone(); c.idle_loops = b.core.idle_loops; c.idle_tail_instructions = b.core.idle_tail_instructions; c.serial_loops=b.core.serial_loops; c.serial_tail_instructions=b.core.serial_tail_instructions;
        assert_eq!(format!("{:?}", c), format!("{:?}", b.core));
        assert_eq!(format!("{:?}", a.int), format!("{:?}", b.int));
        assert_eq!(a.cia, b.cia);
        assert_eq!(a.ram, b.ram);
        assert_eq!(a.cpu_last_data, b.cpu_last_data);
        assert_eq!(a.reset_pending, b.reset_pending);
        assert_eq!(a.iecbus.drv_port, b.iecbus.drv_port);
        assert_eq!(a.iecbus.drv_data, b.iecbus.drv_data);
        assert_eq!(a.iecbus.drv_bus, b.iecbus.drv_bus);
        assert_eq!(a.fast_log, b.fast_log);
        assert_eq!(a.fast_dir, b.fast_dir);
        assert_eq!(a.led, b.led);
    }
    #[cfg(feature="delay-loop")]
    fn delay_fixture(x:u8, base:u64) -> Drive1581 {
        let mut d=fixture();
        let mut rom=vec![0xea;0x8000];
        rom[0x2d37..0x2d3e].copy_from_slice(&[0xca,0xd0,0xfd,0xaa,0x4c,0x3b,0xad]);
        d.rom=Rom1581::Owned(rom.into_boxed_slice().try_into().unwrap());
        d.core.reg_pc=0xad37;d.core.reg_x=x;d.core.clk=base;
        d
    }
    #[cfg(feature="delay-loop")]
    fn validate_delay() {
        // Every X, all short slice tails, and clocks across the 32-bit boundary.
        for base in [100, u32::MAX as u64-20, u32::MAX as u64+100] {
            for x in 0..=255u8 {
                for len in 1..=65 {
                    let mut a=delay_fixture(x,base);let mut b=delay_fixture(x,base);
                    run::<true>(&mut a,base+len);run::<false>(&mut b,base+len);equal(&a,&b);
                }
                let mut a=delay_fixture(x,base);let mut b=delay_fixture(x,base);
                for len in (1..1500).step_by(17) {
                    run::<true>(&mut a,base+len);run::<false>(&mut b,base+len);equal(&a,&b);
                }
            }
        }
        // Actual CIA timer writes, alarm boundaries and IRQ delivery.
        for timer in 1..80u8 {
            let mut a=delay_fixture(255,100);let mut b=delay_fixture(255,100);
            for d in [&mut a,&mut b] {
                let clk_ptr=&mut d.core.clk as *mut u64;
                let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                    ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                    number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                    clk_ptr,cpu_last_data:&mut d.cpu_last_data};
                bus.write(0x4004,timer);bus.write(0x4005,0);bus.write(0x400d,0x81);bus.write(0x400e,1);
            }
            for stop in (101..900).step_by(11) {
                run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
            }
        }
        // Guard rejection, with one direct successful batch as a positive control.
        for kind in 0..10 {
            let mut d=delay_fixture(12,100);
            match kind {
                1 => d.int.set_irq(0,true,100),
                2 => {d.int.global_pending_int |= crate::drive_6510core::IK_NMI;d.int.nmi_clk=100;},
                3 => d.core.is_jammed=true,
                4 => d.core.d_bank_limit=0x1000,
                5 => d.core.reg_x=1,
                6..=9 => if let Rom1581::Owned(ref mut rom)=d.rom {rom[0x2d37+kind-6]^=1;},
                _ => (),
            }
            let clk_ptr=&mut d.core.clk as *mut u64;
            let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                clk_ptr,cpu_last_data:&mut d.cpu_last_data};
            assert_eq!(skip_rom_delay(&mut d.core,&mut d.int,200,&mut bus),kind==0);
        }
    }
    #[cfg(feature="job-scan")]
    fn job_fixture(x:u8,y:u8,base:u64) -> Drive1581 {
        let mut d=fixture();let mut rom=vec![0xea;0x8000];
        rom[0x3117..0x3129].copy_from_slice(&[0xb5,0xa8,0xc9,0xff,0xd0,0x14,
            0x88,0x30,0x05,0xb9,0x02,0x00,0x30,0x0c,0xca,0x10,0xef,0xa5]);
        d.rom=Rom1581::Owned(rom.into_boxed_slice().try_into().unwrap());
        d.ram.fill(0);d.ram[0xa8..0x100].fill(0xff);
        d.core.reg_pc=0xb117;d.core.reg_x=x;d.core.reg_y=y;d.core.clk=base;d
    }
    #[cfg(feature="job-scan")]
    fn validate_job_scan() {
        for x in 0..=255u8 { for y in [0,1,2,9,127,128,129,255] {
            for len in 1..=70 {
                let mut a=job_fixture(x,y,100);let mut b=job_fixture(x,y,100);
                run::<true>(&mut a,100+len);run::<false>(&mut b,100+len);equal(&a,&b);
            }
        }}
        for base in [100,u32::MAX as u64-10,u32::MAX as u64+100] {
            for seed in 0..128u32 {
                let mut a=job_fixture(14,9,base);let mut b=job_fixture(14,9,base);
                // Queue exits, status flags, timers and interrupts.
                for d in [&mut a,&mut b] {
                    d.core.reg_p=seed as u8 & !0x82;
                    if seed%4==0 {d.ram[0xa8+(seed as usize%15)]=0x7f;}
                    if seed%4==1 {d.ram[2+(seed as usize%9)]=0x80;}
                    if seed%4==2 {d.int.set_irq(0,true,base);}
                    if seed%4==3 {
                        let clk_ptr=&mut d.core.clk as *mut u64;
                        let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                            ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                            number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                            clk_ptr,cpu_last_data:&mut d.cpu_last_data};
                        bus.write(0x4004,seed as u8);bus.write(0x4005,0);
                        bus.write(0x400d,0x81);bus.write(0x400e,1);
                    }
                }
                for len in (1..400).step_by(11) {
                    run::<true>(&mut a,base+len);run::<false>(&mut b,base+len);equal(&a,&b);
                }
            }
        }
        for y in 0..=255u8 { for len in [18,23,46,100,300] {
            let mut a=job_fixture(14,y,100);let mut b=job_fixture(14,y,100);
            run::<true>(&mut a,100+len);run::<false>(&mut b,100+len);equal(&a,&b);
        }}
        // Direct guards must leave the complete state untouched on rejection.
        for kind in 0..8 {
            let mut d=job_fixture(14,9,100);let mut reference=job_fixture(14,9,100);
            for d in [&mut d,&mut reference] {match kind {
                0 => d.core.is_jammed=true,
                1 => d.core.d_bank_limit=0x1000,
                2 => d.core.d_bank_start=1,
                3 => d.core.d_bank_base=Some(Box::new([0;0x10000])),
                4 => d.int.set_irq(0,true,100),
                5 => {d.int.global_pending_int |= crate::drive_6510core::IK_NMI;d.int.nmi_clk=100;},
                6 => d.cia.irq_events.push((true,100)),
                _ => (),
            }}
            let clk_ptr=&mut d.core.clk as *mut u64;
            let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                clk_ptr,cpu_last_data:&mut d.cpu_last_data};
            assert!(!skip_job_scan(&mut d.core,&mut d.int,if kind==7 {117} else {200},&mut bus));
            equal(&d,&reference);
        }
        // The shortcut must run, and every guarded ROM byte must reject mutation.
        for changed in 0..=18 {
            let mut d=job_fixture(14,9,100);
            if changed<18 {if let Rom1581::Owned(ref mut rom)=d.rom {rom[0x3117+changed]^=1;}}
            let clk_ptr=&mut d.core.clk as *mut u64;
            let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                clk_ptr,cpu_last_data:&mut d.cpu_last_data};
            assert_eq!(skip_job_scan(&mut d.core,&mut d.int,200,&mut bus),changed==18);
        }
    }
    #[cfg(feature="serial-wait")]
    fn serial_fixture(pc:u16, base:u64) -> Drive1581 {
        let mut d=fixture();d.ram.fill(0xea);
        let (op,dummy) = match pc {0x064b=>(0xf0,0x8c),0x065d=>(0xd0,0xca),0x0676=>(0xf0,0xad),0x0683=>(0xd0,0xad),_=>panic!()};
        d.ram[pc as usize-2..pc as usize+8].copy_from_slice(&[0xa9,4,0x2c,1,0x40,op,0xfb,dummy,1,0x40]);
        let exit=pc as usize+if dummy==0xca {6} else {8};
        d.ram[exit..exit+3].copy_from_slice(&[0x4c,exit as u8,(exit>>8) as u8]);
        d.core.reg_pc=pc-2;d.core.reg_a=0;d.core.reg_y=0;d.core.clk=base;
        d.iecbus.drv_port=if op==0xf0 {0x85} else {0x81};d
    }
    #[cfg(feature="serial-wait")]
    fn validate_serial_wait() {
        let mut loops=0;let mut tails=0;
        for pc in [0x064b,0x065d,0x0676,0x0683] {
            for base in [100,u32::MAX as u64-20,u32::MAX as u64+100] {
                for len in 1..300 {
                    let mut a=serial_fixture(pc,base);let mut b=serial_fixture(pc,base);
                    run::<true>(&mut a,base+len);run::<false>(&mut b,base+len);equal(&a,&b);
                    loops+=a.core.serial_loops;tails+=a.core.serial_tail_instructions;
                }
                // Rising/falling edges at every short slice length and irregular
                // caller boundaries. Input changes only between drive slices.
                for seed in 1..65u32 {
                    let mut a=serial_fixture(pc,base);let mut b=serial_fixture(pc,base);
                    let mut stop=base;let mut random=seed;
                    for i in 0..60 {
                        random=random.wrapping_mul(1664525).wrapping_add(1013904223);
                        stop+=1+(random%31) as u64;
                        if i==30 {a.iecbus.drv_port^=4;b.iecbus.drv_port^=4;}
                        run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
                    }
                }
            }
            // Held masked IRQs must remain pending and otherwise unchanged;
            // NMI/reset and IRQPEND-only cases must still use the interpreter.
            for interrupt in 0..6 { for delay in 0..16 {
                let mut a=serial_fixture(pc,100);let mut b=serial_fixture(pc,100);
                for d in [&mut a,&mut b] {
                    d.core.reg_p |= 4;
                    d.int.set_irq(0,true,100+delay);
                    match interrupt {
                        0 => d.int.interrupt_ack_irq(),
                        1 => d.core.reg_p &= !4,
                        2 => {d.int.global_pending_int |= crate::drive_6510core::IK_NMI;d.int.nmi_clk=100+delay;},
                        3 => d.int.global_pending_int |= crate::drive_6510core::IK_IRQPEND,
                        4 => {d.int.global_pending_int=crate::drive_6510core::IK_IRQPEND;d.int.irq_pending_clk=100+delay;},
                        5 => d.reset(0),
                        _ => (),
                    }
                }
                for stop in (101..500).step_by(5) {
                    if stop==301 {a.int.set_irq(0,false,300);b.int.set_irq(0,false,300);}
                    run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
                }
            }}
            // All composed port values, DDR values, N/V and both branch senses.
            for v in 0..=255u8 {
                let mut a=serial_fixture(pc,100);let mut b=serial_fixture(pc,100);
                for d in [&mut a,&mut b] {d.iecbus.drv_port=v;d.cia.c_cia[1]=v;d.cia.c_cia[3]=!v;}
                run::<true>(&mut a,400);run::<false>(&mut b,400);equal(&a,&b);
            }
            for mode in [1u8,3,0x11,0x13] { for timer in 1..65u8 {
                let mut a=serial_fixture(pc,100);let mut b=serial_fixture(pc,100);
                for d in [&mut a,&mut b] {
                    let clk_ptr=&mut d.core.clk as *mut u64;
                    let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                        ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                        number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                        clk_ptr,cpu_last_data:&mut d.cpu_last_data};
                    bus.write(0x4004,timer);bus.write(0x4005,0);bus.write(0x400d,0x81);bus.write(0x400e,mode);
                }
                for stop in (101..600).step_by(7) {run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);}
            }}
            // Establish the first BIT through the interpreter. Guard failures
            // must reject malformed code/flags, interrupts and banked fetches.
            for kind in 0..29 {
                let mut d=serial_fixture(pc,100);run::<false>(&mut d,106);
                match kind {
                    1..=8 => d.ram[(pc-2) as usize+kind-1]^=1,
                    9 => d.core.reg_a=1,
                    10 => d.core.is_jammed=true,
                    11 => d.core.d_bank_start=1,
                    12 => d.core.d_bank_limit=1,
                    13 => d.core.d_bank_base=Some(Box::new([0;0x10000])),
                    14 => {d.core.reg_p &= !4;d.int.set_irq(0,true,d.core.clk);},
                    15 => {d.int.global_pending_int |= crate::drive_6510core::IK_NMI;d.int.nmi_clk=d.core.clk;},
                    16 => d.cia.irq_events.push((true,d.core.clk)),
                    17 => d.core.last_opcode_addr^=1,
                    18 => d.core.last_opcode_info^=1,
                    19 => d.int.last_opcode_info_ptr^=1,
                    20 => d.cia.last_read^=1,
                    21 => d.core.flag_z^=4,
                    22 => d.core.flag_n^=0x80,
                    23 => d.core.reg_p^=0x40,
                    24 => d.cia.c_cia[15]|=2,
                    25 => {d.core.reg_p|=4;d.int.set_irq(0,true,d.core.clk);},
                    26 => {d.core.reg_p|=4;d.int.set_irq(0,true,d.core.clk);d.int.interrupt_ack_irq();},
                    27 => {d.core.reg_p|=4;d.int.global_pending_int=crate::drive_6510core::IK_IRQPEND;},
                    28 => {d.core.reg_pc=pc;d.core.last_opcode_info=0x278;d.int.last_opcode_info_ptr=0x278;d.core.reg_p|=4;d.int.set_irq(0,true,d.core.clk);},
                    _ => (),
                }
                let clk_ptr=&mut d.core.clk as *mut u64;
                let mut bus=Bus1581 {ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                    ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                    number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                    clk_ptr,cpu_last_data:&mut d.cpu_last_data};
                assert_eq!(skip_serial_wait(&mut d.core,&mut d.int,400,&mut bus),kind==0 || kind==25 || kind==26);
            }
        }
        assert!(loops>10000 && tails>1000,"serial shortcut and tails must execute");
    }
    pub fn validate() {
        #[cfg(feature="serial-wait")] validate_serial_wait();
        #[cfg(feature="job-scan")] validate_job_scan();
        #[cfg(feature="delay-loop")] validate_delay();
        // Irregular IEC time slices, including clocks crossing 32-bit range.
        // Compare complete CPU/CIA/bus state after every slice, not only output.
        for base in [100u64, u32::MAX as u64 - 100, u32::MAX as u64 + 100] {
            for seed in 1..25u32 {
                let mut a=fixture(); let mut b=fixture();
                a.core.clk=base; b.core.clk=base;
                let mut stop=base; let mut random=seed;
                for i in 0..128 {
                    random=random.wrapping_mul(1664525).wrapping_add(1013904223);
                    stop += 1 + (random % 127) as u64;
                    if i==91 {a.iecbus.drv_port^=1;b.iecbus.drv_port^=1;}
                    run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
                }
            }
        }
        let mut skips = 0;
        let mut tails = 0;
        // Every possible stopping point, including partial BIT/BEQ instructions.
        for stop in 100..700 {
            let mut a=fixture(); let mut b=fixture();
            run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
            skips += a.core.idle_loops;
            tails += a.core.idle_tail_instructions;
        }
        assert!(tails > 100, "test must exercise instruction tails");
        assert!(skips > 1000, "test must actually exercise batching");
        // Differential evolution, with serial changes and an IRQ edge between
        // slices, including transition out of the waiting loop.
        let mut a=fixture();let mut b=fixture();
        for i in 1..200 {
            if i==70 { a.iecbus.drv_port^=1; b.iecbus.drv_port^=1; }
            if i==90 { a.atn_flag();b.atn_flag(); }
            let stop=100+i*13;
            run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
        }
        // Timer alarm close to every instruction boundary, plus PB timer output
        // modes (must fall back). Use the actual CIA store path.
        for mode in [1u8,3,0x11,0x13] {
            for timer in 1..80u8 {
                let mut a=fixture();let mut b=fixture();
                for d in [&mut a,&mut b] {
                    let clk_ptr=&mut d.core.clk as *mut u64;
                    let mut bus=Bus1581 { ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                        ports:Ports { fdc:BuiltIn {wd:&mut d.wd,read_only:true},
                            iecbus:&mut d.iecbus,number:0,fast_dir:&mut d.fast_dir,
                            fast_log:&mut d.fast_log,led:&mut d.led,clk:0 },
                        clk_ptr,cpu_last_data:&mut d.cpu_last_data };
                    bus.write(0x4004,timer);bus.write(0x4005,0);
                    bus.write(0x400d,0x81);bus.write(0x400e,mode);
                }
                for stop in (113..800).step_by(13) {
                    run::<true>(&mut a,stop);run::<false>(&mut b,stop);equal(&a,&b);
                }
            }
        }
        // Input bits, N/V flag effects, DDR contents and IRQ/NMI delivery.
        for value in 0..256u16 {
            let mut a=fixture();let mut b=fixture();
            for d in [&mut a,&mut b] {
                d.iecbus.drv_port=value as u8;
                d.cia.c_cia[1]=value as u8;
                d.cia.c_cia[3]=!value as u8;
            }
            run::<true>(&mut a,400);run::<false>(&mut b,400);equal(&a,&b);
        }
        for kind in 0..3 {
            let mut a=fixture();let mut b=fixture();
            run::<true>(&mut a,200);run::<false>(&mut b,200);equal(&a,&b);
            for d in [&mut a,&mut b] {
                if kind==0 {d.int.set_irq(0,true,d.core.clk);}
                if kind==1 {d.int.global_pending_int |= crate::drive_6510core::IK_NMI;d.int.nmi_clk=d.core.clk;}
                if kind==2 {d.reset(0);}
            }
            run::<true>(&mut a,600);run::<false>(&mut b,600);equal(&a,&b);
        }
        // Changed uploaded instructions must never enable the shortcut.
        for offset in 0x6e1..0x6ea {
            let mut d=fixture();run::<false>(&mut d,109);
            d.ram[offset]^=1;
            let before=d.core.idle_loops;
            // Directly test the guard, without executing malformed test code.
            let clk_ptr=&mut d.core.clk as *mut u64;
            let mut bus=Bus1581 { ram:&mut d.ram,rom:&d.rom,cia:&mut d.cia,
                ports:Ports {fdc:BuiltIn {wd:&mut d.wd,read_only:true},iecbus:&mut d.iecbus,
                    number:0,fast_dir:&mut d.fast_dir,fast_log:&mut d.fast_log,led:&mut d.led,clk:0},
                clk_ptr,cpu_last_data:&mut d.cpu_last_data };
            assert!(!skip_data_wait(&mut d.core,&mut d.int,1000,&mut bus));
            assert_eq!(d.core.idle_loops,before);
        }
    }
}

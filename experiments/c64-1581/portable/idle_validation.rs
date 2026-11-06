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
        run_slice_impl::<_, OPT>(&mut d.core, &mut d.int, &mut d.reset_pending, stop, bus);
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
        let mut c = a.core.clone(); c.idle_loops = b.core.idle_loops; c.idle_tail_instructions = b.core.idle_tail_instructions;
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
    pub fn validate() {
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

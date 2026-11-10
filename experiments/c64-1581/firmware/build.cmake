set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${TOP_DIR}/experiments/c64-1581/firmware/prepare.py
    ${TOP_DIR}/experiments/c64-1581/firmware/lib.rs
    ${TOP_DIR}/experiments/c64-1581/portable/profile.rs
    ${TOP_DIR}/experiments/c64-1581/portable/serial_wait.rs
    ${TOP_DIR}/experiments/c64-1581/portable/job_scan.rs
    ${TOP_DIR}/experiments/c64-1581/portable/delay_loop.rs
    ${TOP_DIR}/experiments/c64-1581/portable/idle_wait.rs
    ${TOP_DIR}/experiments/c64-1581/portable/idle_validation.rs
    ${TOP_DIR}/experiments/c64-1581/portable/prepare.py
    ${TOP_DIR}/experiments/c64-1581/bridge/prepare.py)
set(C64_TRX64_SOURCE "${TOP_DIR}/deps/trx64-1581" CACHE PATH "Pinned TRX64 checkout for experimental 1581")
find_program(C64_CARGO cargo REQUIRED)
set(DRIVE1581_DIR ${CMAKE_CURRENT_BINARY_DIR}/drive1581)
set(DRIVE1581_GLUE ${TOP_DIR}/experiments/c64-1581/firmware)
option(C64_1581_IDLE_SRAM "Experimental small 1581 waiting-loop handler in SRAM" OFF)
option(C64_1581_DRIVE_PROFILE "Sample drive CPU/idle/alarm/synchronisation costs" OFF)
set(C64_1581_FLUSH_LINES "1" CACHE STRING "Bounded raster lines between unobserved drive flushes")
if(NOT C64_1581_FLUSH_LINES MATCHES "^(1|4|8|16)$")
    message(FATAL_ERROR "C64_1581_FLUSH_LINES must be 1, 4, 8 or 16")
endif()
set(DRIVE1581_PREPARE_FLAGS --flush-lines ${C64_1581_FLUSH_LINES})
option(C64_1581_SERIAL_WAIT "Batch guarded active loader CLOCK-edge waits" OFF)
if(C64_1581_SERIAL_WAIT)
    list(APPEND DRIVE1581_PREPARE_FLAGS --serial-wait)
endif()
option(C64_1581_JOB_SCAN "Batch guarded ROM job queue scans" OFF)
if(C64_1581_JOB_SCAN)
    list(APPEND DRIVE1581_PREPARE_FLAGS --job-scan)
endif()
option(C64_1581_DELAY_LOOP "Batch guarded ROM DEX/BNE delay iterations" OFF)
if(C64_1581_DELAY_LOOP)
    list(APPEND DRIVE1581_PREPARE_FLAGS --delay-loop)
endif()
option(C64_SID_FAST_STATE "Allocate SID renderer working state in internal SRAM" OFF)
option(C64_SID_HOT_CODE "Execute SID per-sample synthesis from SRAM" OFF)
if(C64_SID_HOT_CODE)
    list(APPEND DRIVE1581_PREPARE_FLAGS --sid-hot-code)
endif()
if(C64_SID_FAST_STATE)
    list(APPEND DRIVE1581_PREPARE_FLAGS --sid-fast-state)
endif()
if(C64_1581_DRIVE_PROFILE)
    list(APPEND DRIVE1581_PREPARE_FLAGS --drive-profile)
endif()
if(C64_1581_IDLE_SRAM)
    list(APPEND DRIVE1581_PREPARE_FLAGS --idle-sram)
endif()
execute_process(COMMAND python3 ${DRIVE1581_GLUE}/prepare.py ${DRIVE1581_PREPARE_FLAGS} --output ${DRIVE1581_DIR} --trx64 ${C64_TRX64_SOURCE} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not prepare experimental 1581")
endif()
execute_process(COMMAND python3 ${TOP_DIR}/scripts/fetch-1581-rom.py RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not verify 1581 ROM")
endif()
set(FRANK ${DRIVE1581_DIR}/core)
set(DRIVE1581_LIB ${DRIVE1581_DIR}/rust/target/thumbv8m.main-none-eabi/release/libretro_1581_firmware.a)
add_custom_command(OUTPUT ${DRIVE1581_LIB}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${DRIVE1581_DIR}/rust/target ${C64_CARGO} build --release --target thumbv8m.main-none-eabi --manifest-path ${DRIVE1581_DIR}/rust/Cargo.toml
    DEPENDS ${DRIVE1581_DIR}/rust/src/lib.rs
    VERBATIM)
option(C64_1581_CPU_SRAM "Experimental 1581 instruction dispatcher in SRAM" OFF)
if(C64_1581_CPU_SRAM)
    set(DRIVE1581_FLASH_LIB ${DRIVE1581_LIB})
    set(DRIVE1581_LIB ${DRIVE1581_DIR}/libretro_1581_sram.a)
    add_custom_command(OUTPUT ${DRIVE1581_LIB}
        COMMAND python3 ${DRIVE1581_GLUE}/place_cpu_sram.py
            --objdump ${CMAKE_OBJDUMP} --objcopy ${CMAKE_OBJCOPY}
            --input ${DRIVE1581_FLASH_LIB} --output ${DRIVE1581_LIB}
        DEPENDS ${DRIVE1581_FLASH_LIB} ${DRIVE1581_GLUE}/place_cpu_sram.py
        VERBATIM)
endif()
option(C64_1581_LOADING_SRAM "Place drive scheduler and mapped reads in SRAM" OFF)
if(C64_1581_LOADING_SRAM)
    set(DRIVE1581_LOADING_INPUT ${DRIVE1581_LIB})
    set(DRIVE1581_LIB ${DRIVE1581_DIR}/libretro_1581_loading_sram.a)
    add_custom_command(OUTPUT ${DRIVE1581_LIB}
        COMMAND python3 ${DRIVE1581_GLUE}/place_loading_sram.py
            --objdump ${CMAKE_OBJDUMP} --objcopy ${CMAKE_OBJCOPY}
            --input ${DRIVE1581_LOADING_INPUT} --output ${DRIVE1581_LIB}
        DEPENDS ${DRIVE1581_LOADING_INPUT} ${DRIVE1581_GLUE}/place_loading_sram.py
        VERBATIM)
endif()
add_custom_target(c64_drive1581 DEPENDS ${DRIVE1581_LIB})
add_custom_command(OUTPUT ${DRIVE1581_DIR}/rom.c
    COMMAND python3 ${TOP_DIR}/src/machines/master/embed.py ${DRIVE1581_DIR}/rom.c retro_1581_rom=${TOP_DIR}/roms/1581/dos1581-318045-02.bin
    DEPENDS ${TOP_DIR}/roms/1581/dos1581-318045-02.bin)

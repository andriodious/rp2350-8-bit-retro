"""Check that diagnostic call sites bracket the operations being investigated."""
import re


def body(source, marker):
    start = source.index(marker)
    start = source.index('{', start)
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start + 1:end - 1]


def ordered(source, *tokens):
    position = 0
    for token in tokens:
        position = source.index(token, position) + len(token)


def verify(root):
    memory = (root / 'src/common/mem.c').read_text()
    callback = body(memory, '__no_inline_not_in_flash_func(write_sector)')
    ordered(callback, 'uint32_t timing = qmi_hw->m[0].timing;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_BEGIN, s_w.offset)',
            'flash_range_erase(', 'flash_range_program(',
            'qmi_hw->m[0].timing = timing;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_END, s_w.offset)')
    assert callback.count('c64_runtime_diag_capture(') == 2
    assert 'c64_runtime_diag_json' not in callback and 'c64_runtime_diag_init' not in callback
    assert 'flash_safe_execute(write_sector, NULL, 1000)' in memory

    menu = (root / 'src/common/boot_banner.c').read_text()
    opening = body(menu, 'static bool status_open(void)')
    ordered(opening, 'if(!status_allocate())return false;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_OPEN_BEGIN, 0)',
            'emulator_rows = board_display_get_rows()',
            'machine_status_framebuffer(true)', 'status_draw(', 'status_present()',
            'status_visible = true;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_OPEN_END, 0)')
    closing = body(menu, 'static void status_close(void)')
    ordered(closing, 'c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_CLOSE_BEGIN, 0)',
            'setup_end()', 'memcpy(status_sram,', 'board_display_set_rows(emulator_rows)',
            'retire_display_table()', 'machine_status_framebuffer(false)',
            'audio_status_mute(false)', 'status_visible = false;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_MENU_CLOSE_END, 0)')
    assert opening.count('c64_runtime_diag_capture(') == 2
    assert closing.count('c64_runtime_diag_capture(') == 2

    main = (root / 'src/machines/c64/main.cpp').read_text()
    ordered(body(main, 'int main()'), 'c64_startup_init()', 'c64_runtime_diag_init()',
            'c64_init()', 'frontend_init()', 'multicore_launch_core1(core1)',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_BOOT_READY, 0)')
    assert main.count('C64_RUNTIME_DIAG_BOOT_READY') == 1

    api = (root / 'src/common/api/api.c').read_text()
    handler = body(api, 'static void cmd_c64_runtime(api_req_t *r)')
    assert re.search(r'#if\s+C64_RUNTIME_DIAG\s+static void cmd_c64_runtime', api)
    assert re.search(r'#if\s+C64_RUNTIME_DIAG\s+\{"c64-runtime",\s*NULL,\s*false,\s*cmd_c64_runtime\}', api)
    assert 'c64_runtime_diag_json(r->out, r->size)' in handler and 'r->code = 200;' in handler
    assert 'c64_runtime_diag_json(j, size)' in handler
    assert not re.search(r'\b(?:c64_runtime_diag_capture|c64_runtime_diag_init|mem_flash_write_sector)\s*\(', handler)
    assert 'c64_runtime_diag_summary_json(j+n,size-n)' in api
    api_header = (root / 'src/common/api/api.h').read_text()
    assert re.search(r'bool\s+post;\s*// HTTP: POST \(it changes something\), else GET', api_header)

    cmake = (root / 'src/machines/c64/machine.cmake').read_text()
    assert re.search(r'option\(C64_RUNTIME_DIAG\s+"[^"]+"\s+OFF\)', cmake)
    assert re.search(r'if\(C64_RUNTIME_DIAG\)\s+target_sources\(presto_c64 PRIVATE \$\{MACHINE_DIR\}/runtime_diag.c\)\s+'
                     r'target_compile_definitions\(presto_c64 PRIVATE C64_RUNTIME_DIAG=1\)\s+endif\(\)', cmake)

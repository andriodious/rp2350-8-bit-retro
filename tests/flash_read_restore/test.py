#!/usr/bin/env python3
"""Run actual mem.c against SDK/flash/lockout mocks under ASan and UBSan.

The preserved diagnostic source must first fail the captured quad-read regression.
The current source must preserve both serial and quad-read settings in every path.
Only temporary compilation files and requested build logs are generated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path,
                    default=ROOT.parent / 'c64-d81-runtime-diag-20261122/src/common/mem.c')
parser.add_argument('--log-dir', type=Path, default=ROOT / 'build-runtime-diag')
args = parser.parse_args()
subject = ROOT / 'src/common/mem.c'


def compile_subject(directory, source, label):
    shutil.copyfile(source, directory / 'mem_subject.c')
    binary = directory / ('test-' + label)
    command = ['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
               '-fno-omit-frame-pointer', '-DBOOT_STATUS_TRIAL=1', '-DC64_RUNTIME_DIAG=1',
               '-DMACHINE_C64=1', '-I' + str(directory), '-I' + str(ROOT / 'src/common'),
               str(directory / 'host.c'), '-o', str(binary)]
    compiled = subprocess.run(command, text=True, capture_output=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    assert not compiled.stderr, compiled.stderr
    return binary, command


def run(binary, *flags):
    result = subprocess.run([str(binary), *flags], text=True, capture_output=True)
    assert not result.stderr, result.stderr  # Includes recoverable UBSan reports.
    return result


def ordered(source, *tokens):
    position = 0
    for token in tokens:
        position = source.index(token, position) + len(token)


def source_order_check():
    text = subject.read_text()
    callback = text[text.index('static void __no_inline_not_in_flash_func(write_sector)'):
                    text.index('static uint32_t s_writes;')]
    ordered(callback, 'uint32_t timing = qmi_hw->m[0].timing;',
            'uint32_t rcmd = qmi_hw->m[0].rcmd;', 'uint32_t rfmt = qmi_hw->m[0].rfmt;',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_BEGIN, s_w.offset)',
            'flash_range_erase(', 'flash_range_program(', 'flash_range_program(',
            '__dsb();', 'qmi_hw->m[0].timing = timing;', 'qmi_hw->m[0].rcmd = rcmd;',
            'qmi_hw->m[0].rfmt = rfmt;', '__dsb();', '__isb();',
            'c64_runtime_diag_capture(C64_RUNTIME_DIAG_FLASH_END, s_w.offset)')
    assert callback.count('__dsb();') == 2 and callback.count('__isb();') == 1
    assert 'qmi_hw->m[1]' not in callback


args.log_dir.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='flash-read-restore-') as temporary:
    directory = Path(temporary)
    for header in ['pico/stdlib.h', 'pico/flash.h', 'pico/multicore.h', 'hardware/flash.h', 'hardware/sync.h',
                   'hardware/structs/qmi.h', 'boot_banner.h', 'board_config.h',
                   'system.h', 'machine_if.h']:
        target = directory / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "mock.h"\n')
    shutil.copyfile(HERE / 'mock.h', directory / 'mock.h')
    shutil.copyfile(HERE / 'host.c', directory / 'host.c')

    old_binary, old_command = compile_subject(directory, args.baseline, 'before')
    before = run(old_binary, '--probe')
    (args.log_dir / 'flash-read-restore-before.log').write_text(
        f'Source: {args.baseline}\nExpected regression exit status: 1\nActual exit status: {before.returncode}\n'
        + before.stdout)
    assert before.returncode == 1, 'Preserved source must demonstrate the regression first.'
    old_values = json.loads(before.stdout.splitlines()[0])
    assert old_values['before'] == {'timing': '60007203', 'rfmt': '000492a8', 'rcmd': '000000eb'}
    assert old_values['after'] == {'timing': '60007203', 'rfmt': '00001000', 'rcmd': '00000003'}
    assert old_values['diagnostic_after'] == old_values['after']
    assert old_values['timing_restored'] and not old_values['read_mode_restored'] and old_values['m1_unchanged']
    print('EXPECTED FAILURE: preserved diagnostic callback reproduces RFMT 0x492a8→0x1000 and RCMD 0xeb→0x03.')

    source_order_check()
    binary, command = compile_subject(directory, subject, 'after')
    after_probe = run(binary, '--probe')
    assert after_probe.returncode == 0, after_probe.stdout
    new_values = json.loads(after_probe.stdout.splitlines()[0])
    assert new_values['before'] == new_values['after'] == new_values['diagnostic_after']
    assert new_values['read_mode_restored'] and new_values['m1_unchanged']
    suite = run(binary)
    assert suite.returncode == 0, suite.stdout
    after_log = (f'Source: {subject}\nProbe exit status: {after_probe.returncode}\nSuite exit status: {suite.returncode}\n'
                 + after_probe.stdout + suite.stdout
                 + 'PASS: source order is final SDK call → DSB → timing → command → format → DSB → ISB → diagnostic end.\n'
                 + 'ASan/UBSan stderr: empty\n')
    (args.log_dir / 'flash-read-restore-after.log').write_text(after_log)
    provenance = {
        'baseline_source': str(args.baseline),
        'baseline_source_sha256': hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
        'current_source': str(subject),
        'current_source_sha256': hashlib.sha256(subject.read_bytes()).hexdigest(),
        'sources_compiled_byte_for_byte': True,
        'before_compile_command': old_command,
        'after_compile_command': command,
        'compiler': subprocess.check_output(['cc', '--version'], text=True).splitlines()[0],
        'baseline_expected_failure_exit': before.returncode,
        'current_probe_exit': after_probe.returncode,
        'current_suite_exit': suite.returncode,
        'before_values': old_values,
        'after_values': new_values,
        'sanitizer_stderr': before.stderr + after_probe.stderr + suite.stderr,
        'scope': 'Actual callback and outer mem_flash_write_sector with mocked SDK re-entry, register words, SRAM input, flash array, IRQ state and multicore lockout; no device access.',
    }
    (args.log_dir / 'flash-read-restore-provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(after_probe.stdout, end='')
    print(suite.stdout, end='')
    print('PASS: restore/barrier source order; ASan/UBSan clean.')

#!/usr/bin/env python3
"""Exercise the actual runtime collector with read-only hardware mocks and sanitizers.

No firmware is built or hardware accessed. Every generated file is temporary.
The Python JSON parser validates every distinct result from every buffer capacity.
"""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from integration import verify as verify_integration

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SUBJECT = ROOT / 'src/machines/c64/runtime_diag.c'
COMMON = ROOT / 'src/common'
SANITIZERS = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
verify_integration(ROOT)


def invoke(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f"Command failed ({result.returncode}): {' '.join(map(str, command))}\n"
                             + result.stdout + result.stderr)
    # A successful process must not conceal a recoverable UBSan report.
    assert not result.stderr, result.stderr
    return result.stdout


def full(records, label):
    return next(r['value'] for r in records if r['label'] == label and r['kind'] == 'full')


def summary(records, label):
    return next(r['value']['c64_runtime'] for r in records
                if r['label'] == label and r['kind'] == 'summary')


def expected_snapshot(seed, sequence, event, arg):
    return {
        'sequence': sequence, 'event': event, 'arg': arg,
        'timeUs': ((0x80000000 + seed) << 32) | (0xffff0000 + seed),
        'm0': [0xf0000000 + seed * 256 + i for i in range(5)],
        'm1': [0xf0000010 + seed * 256 + i for i in range(5)],
        'xipCtrl': 0xe0000000 + seed,
        'bootram': [0xd0000000 + seed * 256 + i for i in range(5)],
        'clocks': [0xc0000000 + seed, 0xc1000000 + seed,
                   0xc2000000 + seed, 0xc3000000 + seed],
        'devinfo': 0xa000 + seed,
    }


def check_common(records, scenario):
    before = full(records, 'before_init')
    after = full(records, 'after_init')
    assert before['initialized'] is False and after['initialized'] is True
    for value in (before, after):
        assert value['schemaVersion'] == 1
        assert value['sequence'] == 0 and value['events'] == [] and value['baseline'] is None
        assert value['eventCapacity'] == 8 and value['overwritten'] == 0
        assert value['configuredSysHz'] == 250000000
        assert value['flashWrites'] == 23 and value['lcd'] == [101, 202, 303, 404]
    assert summary(records, 'after_init')['changed'] is None
    base = full(records, 'baseline')
    assert base['sequence'] == 1 and len(base['events']) == 1
    assert base['baseline'] == base['events'][0]
    assert summary(records, 'baseline')['changed'] == 0
    valid = scenario in {'normal', 'sram', 'bootram', 'sram_last', 'bootram_last', 'max'}
    expected = (65535 if scenario == 'max' else 0xa001) if valid else None
    assert base['baseline']['devinfo'] == expected
    assert (base['devinfoAddress'] != 0) == valid
    assert base['labels']['qmi'] == ['timing', 'rfmt', 'rcmd', 'wfmt', 'wcmd']
    assert base['labels']['events'] == ['live', 'boot_ready', 'menu_open_begin', 'menu_open_end',
                                      'menu_close_begin', 'menu_close_end', 'flash_begin', 'flash_end']
    for record in records:
        if record['label'] != 'bounded':
            continue
        value = record['value']
        if record['kind'] == 'full':
            assert value is not None or record['cap'] < 3
            assert value is None or isinstance(value, dict)
            if value and 'error' in value:
                assert value == {'schemaVersion': 1, 'error': 'buffer_too_small'}
        else:
            assert value['anchor'] is True
            if 'c64_runtime' in value and 'error' in value['c64_runtime']:
                assert value['c64_runtime'] == {'error': 'buffer_too_small'}


with tempfile.TemporaryDirectory(prefix='c64-runtime-diag-') as temporary:
    directory = Path(temporary)
    for header in ['pico/stdlib.h', 'pico/bootrom.h', 'hardware/clocks.h',
                   'hardware/regs/addressmap.h', 'hardware/structs/clocks.h',
                   'hardware/structs/pll.h', 'hardware/structs/qmi.h',
                   'hardware/structs/timer.h', 'hardware/structs/xip_ctrl.h',
                   'board.h', 'mem.h']:
        target = directory / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "mock.h"\n')
    shutil.copyfile(HERE / 'mock.h', directory / 'mock.h')
    # Copy the source byte-for-byte: same implementation, only hardware headers mocked.
    shutil.copyfile(SUBJECT, directory / 'runtime_diag_subject.c')
    shutil.copyfile(HERE / 'host.c', directory / 'host.c')
    executable = directory / 'host'
    invoke(['cc', '-std=c11', '-O1', *SANITIZERS, '-DC64_RUNTIME_DIAG=1', '-DMACHINE_C64=1',
            '-I' + str(directory), '-I' + str(COMMON), str(directory / 'host.c'), '-o', str(executable)])
    scenarios = ['normal', 'max', 'sram', 'bootram', 'sram_last', 'bootram_last',
                 'missing_literal', 'null', 'odd', 'sram_past', 'bootram_past', 'outside']
    all_records = {}
    for scenario in scenarios:
        output = invoke([str(executable), scenario])
        records = [json.loads(line) for line in output.splitlines()]
        check_common(records, scenario)
        all_records[scenario] = records

    normal = all_records['normal']
    baseline = expected_snapshot(1, 1, 1, 0x1111)
    assert full(normal, 'baseline')['baseline'] == baseline
    masks = {'time_only': 0, 'm0': 1, 'm1': 2, 'xip': 4, 'bootram': 8,
             'clocks': 16, 'devinfo': 32, 'all': 63}
    for group, expected in masks.items():
        assert summary(normal, 'mask_' + group)['changed'] == expected
    wrapped = full(normal, 'wrapped')
    assert wrapped['sequence'] == 26 and wrapped['overwritten'] == 18
    assert wrapped['baseline'] == baseline
    expected = [expected_snapshot(i + 1, i + 1, 2 + (i - 1) % 6, 0x10000000 + i)
                for i in range(18, 26)]
    assert wrapped['events'] == expected
    assert wrapped['live'] == expected_snapshot(26, 26, 0, 0)
    assert full(normal, 'reinitialized') == wrapped
    assert full(normal, 'after_serialization') == wrapped
    assert summary(normal, 'wrapped')['events'] == 8
    assert summary(normal, 'wrapped')['lastEvent'] == expected[-1]['event']
    counter_wrap = full(normal, 'counter_wrap')
    assert counter_wrap['sequence'] == 0 and counter_wrap['overwritten'] == 0xffffffff
    assert counter_wrap['baseline'] == baseline
    assert [e['sequence'] for e in counter_wrap['events']] == list(range(21, 27)) + [0xffffffff, 0]
    assert [e['arg'] for e in counter_wrap['events'][-2:]] == [0xfffff000, 0xfffff000]
    assert summary(normal, 'counter_wrap')['lastEvent'] == 7

    maximum = full(all_records['max'], 'maximum')
    assert len(maximum['events']) == 8 and maximum['baseline']['arg'] == 0xffffffff
    for snapshot in [maximum['baseline'], *maximum['events'], maximum['live']]:
        assert snapshot['timeUs'] == 0xffffffffffffffff
        for name, count in [('m0', 5), ('m1', 5), ('bootram', 5), ('clocks', 4)]:
            assert snapshot[name] == [0xffffffff] * count
        assert snapshot['xipCtrl'] == 0xffffffff and snapshot['devinfo'] == 65535
    maximum_bytes = len(json.dumps(maximum, separators=(',', ':')).encode()) + 1
    assert maximum_bytes <= 8192

    # Disabled/header-only entry points must compile and link without Pico headers,
    # for both C and C++, with diagnostics off and on non-C64 machines.
    variants = [[], ['-DC64_RUNTIME_DIAG=0', '-DMACHINE_C64=1'],
                ['-DC64_RUNTIME_DIAG=1', '-DMACHINE_C64=0']]
    for language, compiler, standard in [('c', 'cc', 'c11'), ('c++', 'c++', 'c++17')]:
        for index, defines in enumerate(variants):
            binary = directory / f'disabled-{language}-{index}'
            invoke([compiler, '-x', language, '-std=' + standard, *SANITIZERS, *defines,
                    '-I' + str(COMMON), str(HERE / 'disabled.c'), str(SUBJECT), '-o', str(binary)])
            invoke([str(binary)])

print('PASS: explicit immutable baseline; ring/counter wrap; independent change masks; read-only capture; '
      '12 pointer/value scenarios; bounded full/summary JSON; C/C++ disabled stubs; ASan/UBSan; '
      'flash/menu/startup hook order and read-only API registration')
print(f'Max-value full JSON: {maximum_bytes} bytes including NUL (limit 8192).')

"""Verify test build packaging, RAM budget and absence of Bluetooth flash storage.
Usage: python3 tests/bluetooth/firmware_test.py <build-root> <arm-none-eabi-nm>
"""
import importlib.util, pathlib, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[2]
build=pathlib.Path(sys.argv[1]); nm=sys.argv[2]
spec=importlib.util.spec_from_file_location('uf2',root/'tools/dual_boot_uf2.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
expected={}
for machine,name,offset in [('launcher','presto_launcher',0),('master','presto_master',32768),('beeb','presto_beebjit',4128*1024),('c64','presto_c64',8224*1024)]:
    for address,payload in u.blocks(build/machine/(name+'.uf2')):
        assert address+offset not in expected
        expected[address+offset]=payload
    if machine=='launcher':continue
    text=subprocess.check_output([nm,'-C',str(build/machine/(name+'.elf'))],text=True)
    symbols={parts[2]:int(parts[0],16) for line in text.splitlines() if len(parts:=line.split(maxsplit=2))==3}
    assert 'btstack_cyw43_init' in symbols
    assert 'btstack_link_key_db_memory_instance' in symbols
    assert not any('pico_flash_bank' in s or 'btstack_link_key_db_tlv' in s for s in symbols)
    cache=(build/machine/'CMakeCache.txt').read_text();assert 'NIMBUS_TEST:BOOL=ON' in cache
    if machine=='c64':
        gap=symbols['__HeapLimit']-symbols['__end__'];assert gap>=40*1024,gap
        assert 0x20000000<=symbols['MOS6510::EmulateLine(int)']<0x20080000
        assert 0x20000000<=symbols['MOS6569::EmulateLine(int&)']<0x20080000
        print('C64 static headroom:',gap,'bytes (not live free RAM)')
combined=u.blocks(build/'presto_dual_boot.uf2')
assert len(combined)==len(expected) and dict(combined)==expected
print('All three targets use RAM pairing keys; combined UF2 exactly matches',len(expected),'payload blocks')

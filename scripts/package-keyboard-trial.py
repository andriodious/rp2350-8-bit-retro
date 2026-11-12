#!/usr/bin/env python3
"""Replace only Master in a verified four-machine UF2; preserve every other block."""
import hashlib, importlib.util, json, pathlib, sys
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('uf2',root/'tools/dual_boot_uf2.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
reference,master,out=map(pathlib.Path,sys.argv[1:])
original=dict(u.blocks(reference));lo=u.FLASH+32768;hi=u.FLASH+4128*1024
kept={a:b for a,b in original.items() if not lo<=a<hi}
replacement={a+32768:b for a,b in u.blocks(master)}
assert replacement and all(lo<=a<hi for a in replacement)
assert not kept.keys()&replacement.keys()
u.write(out,sorted((kept|replacement).items()))
actual=dict(u.blocks(out))
assert {a:b for a,b in actual.items() if not lo<=a<hi}==kept
assert {a:b for a,b in actual.items() if lo<=a<hi}==replacement
print('Verified unchanged launcher, Beeb, C64 and Spectrum payloads')
print('SHA256',hashlib.sha256(out.read_bytes()).hexdigest())

"""Exercise actual machine HID mappings without booting the hardware."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as d:
 for machine,ext in [('spectrum','c'),('c64','cpp')]:
  source=(root/f'src/machines/{machine}/main.{ext}').read_text()
  start=source.index('void machine_key_sdl('); brace=source.index('{',start); end=brace+1; depth=1
  while depth:
   depth += (source[end]=='{')-(source[end]=='}'); end+=1
  function=source[start:end]
  assertions='machine_key_sdl(4,true);assert(keys[5]);machine_key_sdl(4,false);assert(!keys[5]);machine_key_sdl(80,true);assert(keys[0]&&keys[19]);machine_key_sdl(80,false);assert(!keys[0]&&!keys[19]);machine_key_sdl(42,true);assert(keys[0]&&keys[20]);' if machine=='spectrum' else 'machine_key_sdl(4,true);assert(keys[10]);machine_key_sdl(4,false);assert(!keys[10]);machine_key_sdl(52,true);assert(keys[45]);machine_key_sdl(76,true);assert(keys[0]);'
  p=Path(d)/'test.c';p.write_text('#include <stdint.h>\n#include <stdbool.h>\n#include <assert.h>\nstatic bool keys[256];\nvoid machine_key(int c,bool d){assert(c>=0&&c<256);keys[c]=d;}\n'+function+'\nint main(void){'+assertions+'return 0;}')
  subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p),'-o',str(Path(d)/'test')],check=True)
  subprocess.run([str(Path(d)/'test')],check=True)
  print(machine,'actual HID mapping: PASS')

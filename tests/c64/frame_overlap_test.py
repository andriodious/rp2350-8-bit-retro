"""Compare every indexed frame with overlap OFF/ON and verify reader ownership.
Uses the existing native host harness and sanitizers; does not simulate the LCD.
"""
from pathlib import Path
import subprocess,tempfile,shlex,hashlib
root=Path(__file__).resolve().parents[2]
host=root/'tests/c64/native'
main=(host/'main.cpp').read_text()
insert=r'''
extern "C" uint8_t *c64_exchange_framebuffer();
static uint8_t *reader;
static uint32_t reader_hash;
static unsigned frame_number;
static uint32_t hash_frame(const uint8_t *p){
 uint32_t h=2166136261u;
 for(unsigned i=0;i<384*272;i++)h=(h^p[i])*16777619u;
 return h;
}
static void checked_frame(){
 c64_run_frame();
#if C64_RENDER_OVERLAP
 // The entire next emulated frame must leave the converter's input untouched.
 if(reader && hash_frame(reader)!=reader_hash)abort();
#endif
 uint8_t *current=c64_get_framebuffer();uint32_t h=hash_frame(current);
 printf("FRAME %u %08x\n",frame_number,h);
#if C64_RENDER_OVERLAP
 // Alternate blocks of normal and Turbo-style submission, including skipped
 // exchanges and mode transitions. Ownership returns before any buffer reuse.
 bool submitted=(frame_number/100)%2==0 || (frame_number%2)==0;
 reader=nullptr;
 if(submitted){
  reader=c64_exchange_framebuffer();reader_hash=h;
  if(reader!=current || c64_get_framebuffer()==reader)abort();
 }
#endif
 ++frame_number;
}
'''
prefix, body = main.split('int main(', 1)
main=prefix+insert+'\nint main('+body.replace('c64_run_frame();','checked_frame();')
# Run all original 1000 BASIC and 10000 Elite frames.
script=(host/'build.sh').read_text()
script=script.replace('ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"','ROOT='+shlex.quote(str(root)))
script=script.replace('HOST="$(cd "$(dirname "$0")" && pwd)"','HOST='+shlex.quote(str(host)))
with tempfile.TemporaryDirectory() as td:
 t=Path(td);(t/'main.cpp').write_text(main)
 hashes=[]
 for enabled in (0,1):
  build=script.replace('c++ -std=c++20',f'c++ -DC64_RENDER_OVERLAP={enabled} -std=c++20')
  build=build.replace('"$HOST/main.cpp"',shlex.quote(str(t/'main.cpp')))
  (t/'build.sh').write_text(build)
  result=subprocess.run(['bash',str(t/'build.sh')],stdout=subprocess.PIPE,text=True,check=True)
  frames=[line for line in result.stdout.splitlines() if line.startswith('FRAME ')]
  assert len(frames)==11000,len(frames)
  hashes.append(frames)
 assert hashes[0]==hashes[1], 'Frame output changed with overlap enabled'
 print('11000 BASIC/Elite frame hashes match; converter input remains immutable during next frame')
 print('Frame sequence SHA256:',hashlib.sha256('\n'.join(hashes[0]).encode()).hexdigest())

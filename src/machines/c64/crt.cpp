/* Bounded Presto CRT loader. ROM mapping follows FRANK/Frodo Cartridge.cpp,
 * Frodo Copyright (C) Christian Bauer, GPL-2.0-or-later.
 * EasyFlash register behaviour follows the rh1tech/frank-c64 implementation.
 * ROMs are read-only: flash programming and persistent cartridge saves are not emulated.
 */
#include "sysdeps.h"
#include "Cartridge.h"
#include "C64.h"
#include "CPUC64.h"
#include "psram_allocator.h"
#include "crt.h"
#include <new>
#include <cstring>
extern C64 *TheC64;
static unsigned be16(const uint8_t *p){return (unsigned(p[0])<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return (uint32_t(be16(p))<<16)|be16(p+2);}
class PrestoCart final : public Cartridge {
public:
    uint8_t *rom; bool easy, game; unsigned bank=0,mode=0; uint8_t ram[256]{};
    PrestoCart(uint8_t *r,bool e,bool g):rom(r),easy(e),game(g){Reset();}
    ~PrestoCart(){psram_free(rom);}
    static void operator delete(void *p){psram_free(p);}
    void Reset() override {bank=mode=0;notEXROM=easy;notGAME=easy?false:game;}
    uint8_t ReadROML(uint16_t a,uint8_t r,bool l) override {
        return ((!notEXROM && l) || (notEXROM && !notGAME))?rom[bank*0x4000+(a&0x1fff)]:r;
    }
    uint8_t ReadROMH(uint16_t a,uint8_t r,uint8_t b,bool l,bool h) override {
        if(!notGAME)return (notEXROM || h)?rom[bank*0x4000+0x2000+(a&0x1fff)]:r;
        return l?b:r;
    }
    void WriteIO1(uint16_t a,uint8_t v) override {
        if(!easy)return;
        if(!(a&2))bank=v&63;
        else {mode=v&0x87;notEXROM=!(mode&2);notGAME=(mode&4)?!(mode&1):false;}
    }
    uint8_t ReadIO2(uint16_t a,uint8_t bus) override {return easy?ram[a&255]:bus;}
    void WriteIO2(uint16_t a,uint8_t v) override {if(easy)ram[a&255]=v;}
};
static Cartridge *original;
static PrestoCart *active;
static void install(Cartridge *cart){
    TheC64->TheCart=cart;
    TheC64->TheCPU->SetChips(TheC64->TheVIC,TheC64->TheSID,TheC64->TheCIA1,
        TheC64->TheCIA2,cart,TheC64->TheIEC,TheC64->TheTape);
}
extern "C" bool c64_crt_active(){return active!=nullptr;}
extern "C" void c64_crt_detach(){if(active){install(original);delete active;active=nullptr;}}
extern "C" const char *c64_crt_prepare(const uint8_t *d,uint32_t n){
    if(!TheC64 || !d || n<64 || n>1024*1024)return "Invalid CRT size";
    if(memcmp(d,"C64 CARTRIDGE   ",16) || be16(d+20)!=0x100)return "Unsupported CRT signature/version";
    unsigned type=be16(d+22);bool easy=type==32;
    if((type!=0 && !easy) || d[24]>1 || d[25]>1 || (!easy && d[24]))return "Unsupported CRT hardware (use normal 8/16K or EasyFlash)";
    uint32_t start=be32(d+16);if(start<64 || start>n)return "Invalid CRT header length";
    // Validate every packet before allocating or changing the running machine.
    bool occupied[128]{};unsigned count=0;
    for(uint32_t p=start;p<n;){
        if(n-p<16 || memcmp(d+p,"CHIP",4))return "Truncated CRT CHIP header";
        uint32_t packet=be32(d+p+4);unsigned kind=be16(d+p+8),bank=be16(d+p+10),addr=be16(d+p+12),len=be16(d+p+14);
        if(packet!=16u+len || packet>n-p || !len)return "Invalid CRT CHIP length";
        if((kind!=0 && !(easy && kind==2)) || bank>=(easy?64u:1u))return "Unsupported CRT CHIP type/bank";
        if(addr!=0x8000 && addr!=0xa000 && !(easy && addr==0xe000))return "Unsupported CRT CHIP address";
        bool high=addr!=0x8000;
        if((len!=0x2000 && !(len==0x4000 && !easy && !d[25] && !high)) || (!easy && d[25] && high))return "Unsupported CRT ROM size";
        unsigned slot=bank*2+high;
        if(occupied[slot] || (len==0x4000 && occupied[slot+1]))return "Overlapping CRT CHIP data";
        occupied[slot]=true;if(len==0x4000)occupied[slot+1]=true;
        count++;p+=packet;
    }
    if(!count || !occupied[0] || ((easy || !d[25]) && !occupied[1]))return "CRT missing boot ROM";
    size_t bytes=easy?1024*1024:0x4000;
    auto rom=static_cast<uint8_t *>(psram_malloc(bytes));
    if(!rom)return "Not enough PSRAM for cartridge";
    void *obj=psram_malloc(sizeof(PrestoCart));
    if(!obj){psram_free(rom);return "Not enough PSRAM for cartridge state";}
    memset(rom,255,bytes);
    for(uint32_t p=start;p<n;p+=be32(d+p+4)){
        unsigned slot=be16(d+p+10)*2+(be16(d+p+12)!=0x8000);
        memcpy(rom+slot*0x2000,d+p+16,be16(d+p+14));
    }
    auto next=new(obj) PrestoCart(rom,easy,d[25]!=0);
    if(!original)original=TheC64->TheCart;
    install(next);delete active;active=next;return nullptr;
}

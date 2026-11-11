// Bounded Z80 decompression: dry-run the entire snapshot before changing RAM.
static bool z80_unpack(const uint8_t *src,uint32_t size,uint8_t *dst,uint32_t count){
 uint32_t i=0,o=0;
 while(i<size){uint8_t v=src[i++];uint32_t run=1;
  if(v==0xed&&i<size&&src[i]==0xed){if(size-i<3)return false;run=src[i+1];v=src[i+2];i+=3;if(!run)return false;}
  if(run>count-o)return false;
  if(dst)memset(dst+o,v,run);
  o+=run;
 }
 return o==count;
}
static bool z80_pages(const uint8_t *p,uint32_t n,bool write){
 uint16_t pc=word(p+6);uint32_t off=30;uint8_t flags=p[12]==255?1:p[12];
 if(pc){
  if(flags&16)return false; // SamRam is not a 48K Spectrum.
  if(!(flags&32)){if(n-30!=49152)return false;if(write)memcpy(address_space+16384,p+30,49152);return true;}
  uint32_t len=n-30;
  if(len>=4&&!memcmp(p+n-4,"\x00\xed\xed\x00",4))len-=4;
  return z80_unpack(p+30,len,write?address_space+16384:NULL,49152);
 }
 if(n<32)return false;
 uint32_t ext=word(p+30);if((ext!=23&&ext!=54&&ext!=55)||ext>n-32)return false;
 uint8_t mode=p[34];
 // Accept 48K, inactive Interface 1, or inactive MGT. Reject paged peripherals/16K/128K.
 if((mode!=0&&mode!=1&&!(ext>=54&&mode==3))||p[36]||(p[37]&128)||(ext>=54&&(p[59]||p[60])))return false;
 off=32+ext;unsigned seen=0;
 while(off<n){if(n-off<3)return false;uint32_t len=word(p+off);uint8_t page=p[off+2];off+=3;
  bool raw=len==65535;if(raw)len=16384;if(len>n-off)return false;
  int bank=page==8?0:page==4?1:page==5?2:-1;
  if(bank<0||(seen&(1u<<bank)))return false;
  seen|=1u<<bank;
  uint8_t *dst=write?address_space+16384*(bank+1):NULL;
  if(raw){if(dst)memcpy(dst,p+off,len);}else if(!z80_unpack(p+off,len,dst,16384))return false;
  off+=len;
 }
 return seen==7;
}
bool spectrum_load_z80(const uint8_t *p,uint32_t n){
 if(!p||n<30||(p[29]&3)>2||!z80_pages(p,n,false))return false;
 memset(&tape,0,sizeof tape);autoload_us=0;zx_reset(&zx);web_joystick=controller_joystick=0;z80_pages(p,n,true);
 z80_t *c=&zx.cpu;
 c->a=p[0];c->f=p[1];c->bc=word(p+2);c->hl=word(p+4);c->sp=word(p+8);
 c->i=p[10];uint8_t flags=p[12]==255?1:p[12];c->r=(p[11]&127)|((flags&1)<<7);
 c->de=word(p+13);c->bc2=word(p+15);c->de2=word(p+17);c->hl2=word(p+19);c->af2=(p[21]<<8)|p[22];
 c->iy=word(p+23);c->ix=word(p+25);c->iff1=p[27]!=0;c->iff2=p[28]!=0;c->im=p[29]&3;
 zx.border_color=(flags>>1)&7;zx.last_fe_out=zx.border_color;
 zx.pins=z80_prefetch(c,word(p+6)?word(p+6):word(p+32));memset(zx.physical_keys,0,8);return true;
}
bool spectrum_load_image(const char *name,const uint8_t *p,uint32_t n){
 const char *ext=name?strrchr(name,'.'):NULL;media_error=NULL;
 if(!ext){media_error="Expected .sna, .z80, .tap or .tzx";return false;}
 if(!strcasecmp(ext,".sna"))return spectrum_load_sna(p,n);
 if(!strcasecmp(ext,".z80")){bool ok=spectrum_load_z80(p,n);if(!ok)media_error="Invalid or unsupported Z80 snapshot (48K only)";return ok;}
 bool tzx=!strcasecmp(ext,".tzx");
 if(!tzx&&strcasecmp(ext,".tap")){media_error="Expected .sna, .z80, .tap or .tzx";return false;}
 uint32_t blocks;
 if(!spectrum_tape_validate(p,n,tzx,&blocks,&media_error))return false;
 if(!tape_store||n>tape_capacity){media_error="Tape exceeds available storage";return false;}
 memcpy(tape_store,p,n);spectrum_tape_mount(&tape,tape_store,n,tzx,blocks);
 return spectrum_tape_command("load");
}

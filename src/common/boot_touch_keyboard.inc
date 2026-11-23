#include "boot_touch_keys.h"
static bool touch_keyboard, touch_key_shift, touch_key_symbol, touch_key_extra, touch_key_armed;
static unsigned touch_key_focus, touch_key_count;
static int touch_key_direction;
static uint32_t touch_key_repeat;
// Layout is regenerated only when drawing; input always hits the visible layout.
typedef struct { int16_t x,y,w,h,code; } touch_key_button_t;
static touch_key_button_t touch_key_buttons[80];
enum { TK_RESET=-1,TK_SETTINGS=-2,TK_SHIFT=-3,TK_SYMBOL=-4,TK_MAIN=-5,TK_EXTRA=-6 };
static void touch_key_open(void) {
    touch_keyboard=true;touch_key_shift=touch_key_symbol=touch_key_extra=false;
    touch_key_focus=touch_key_count=0;touch_key_direction=0;touch_key_armed=false;chooser_dirty=true;
}
static const char *touch_key_label(int code) {
    if(code==TK_RESET)return "RESET";
    if(code==TK_SETTINGS)return "SETTINGS";
    if(code==TK_SHIFT)return touch_key_shift?"SHIFT ON":"SHIFT OFF";
    if(code==TK_SYMBOL)return touch_key_symbol?"SYM ON":"SYM OFF";
    if(code==TK_MAIN)return "KEYS";
    if(code==TK_EXTRA)return "MORE KEYS";
    if(code==128)return "BREAK";
    if(code==134)return "RESTORE";
    const char *name=setup_key_name(code);
    if(touch_key_shift) {
        const char *normal="123456789";
        if(strlen(name)==1)for(unsigned i=0;i<9;i++)if(name[0]==normal[i]) {
            static const char *const labels[]={"!","\"","#","$","%","&","'","(",")"};return labels[i];
        }
        if(!strcmp(name,"COMMA"))return "<";
    if(!strcmp(name,"PERIOD"))return ">";
    if(!strcmp(name,"SLASH"))return "?";
        if(code==23)return "=";
    if(code==24)return "~";
    if(code==120)return "|";
    if(code==56)return "{";
        if(code==88)return "}";
    if(code==87)return "+";
    if(code==72)return "*";
    if(code==40)return "POUND";
    }
    if(!strcmp(name,"CAPS LOCK"))return "CAPS";
    if(!strcmp(name,"SHIFT LOCK"))return "LOCK";
    if(!strcmp(name,"PAD RETURN"))return "PAD RET";
    if(!strcmp(name,"PAD DELETE"))return "PAD DEL";
    if(!strcmp(name,"PAD SLASH"))return "PAD /";
    if(!strcmp(name,"PAD STAR"))return "PAD *";
    if(!strcmp(name,"PAD MINUS"))return "PAD -";
    if(!strcmp(name,"PAD PLUS"))return "PAD +";
    if(!strcmp(name,"PAD HASH"))return "PAD #";
    if(!strcmp(name,"PAD COMMA"))return "PAD ,";
    if(!strcmp(name,"PAD PERIOD"))return "PAD .";
    if(!strcmp(name,"RETURN"))return "RET";
    if(!strcmp(name,"ENTER"))return "ENT";
    if(!strcmp(name,"ESCAPE"))return "ESC";
    if(!strcmp(name,"RUN STOP"))return "STOP";
    if(!strcmp(name,"COMMODORE"))return "C=";
    if(!strcmp(name,"CTRL"))return "CTL";
    if(!strcmp(name,"COMMA"))return ",";
    if(!strcmp(name,"PERIOD"))return ".";
    if(!strcmp(name,"SLASH"))return "/";
    if(!strcmp(name,"COLON"))return ":";
    if(!strcmp(name,"SEMICOLON"))return ";";
    if(!strcmp(name,"MINUS"))return "-";
    if(!strcmp(name,"PLUS"))return "+";
    if(!strcmp(name,"EQUAL"))return "=";
    if(!strcmp(name,"STAR"))return "*";
    if(!strcmp(name,"AT"))return "@";
    if(!strcmp(name,"L BRACKET"))return "[";
    if(!strcmp(name,"R BRACKET"))return "]";
    if(!strcmp(name,"CARET"))return "^";
    if(!strcmp(name,"BACKSLASH"))return "BACK SLASH";
    if(!strcmp(name,"UNDERSCORE"))return "_";
    return name;
}
static void touch_key_button(uint32_t *fb,int x,int y,int w,int h,int code) {
    unsigned index=touch_key_count++;
    if(index>=80){touch_key_count=80;return;}
    touch_key_buttons[index]=(touch_key_button_t){x,y,w,h,code};
    // Toggle/page fill reflects state, independently of navigation focus.
    bool state_control=code==TK_SHIFT||code==TK_SYMBOL||code==TK_MAIN||code==TK_EXTRA;
    bool selected=code==TK_SHIFT?touch_key_shift:code==TK_SYMBOL?touch_key_symbol:
        code==TK_MAIN?!touch_key_extra:code==TK_EXTRA?touch_key_extra:index==touch_key_focus;
    box(fb,x,y,w,h,selected?3:(code==TK_RESET||code==TK_SETTINGS)?4:0,7);
    if(state_control&&index==touch_key_focus&&!selected) {
        for(int xx=x+2;xx<x+w-2;xx++){pixel(fb,xx,y+1,3);pixel(fb,xx,y+h-2,3);}
    }
    text_ink=selected?0:7;
    const char *label=touch_key_label(code);
    if(w<80) {
        if(code==TK_SHIFT)label="CAPS";
        else if(code==TK_SYMBOL)label="SYM";
        else if(!strcmp(label,"SPACE"))label="SPC";
        else if(!strcmp(label,"RIGHT"))label="RGT";
        else if(!strcmp(label,"QUOTE"))label="\"";
    }
    unsigned cap=(w-4)/12,len=strlen(label);
    // Narrow physical keys retain the existing readable 10px glyphs. Long
    // legends use two/three lines rather than shrinking the touch target.
    unsigned lines=(len+cap-1)/cap;
    for(unsigned row=0;row<lines;row++) {
        char part[24];unsigned n=len-row*cap;
    if(n>cap)n=cap;
    if(n>=sizeof part)n=sizeof part-1;
        memcpy(part,label+row*cap,n);part[n]=0;
        text(fb,x+(w-(int)n*12)/2,y+(h-(int)lines*8)/2+row*8,part);
    }
    text_ink=7;
}
static void touch_key_draw(uint32_t *fb) {
    const touch_key_row_t *rows;unsigned count;
#define TK_ROWS(name) rows=name;count=sizeof name/sizeof *name
    if(!strcmp(machine_name(),"beeb")) {
        if(touch_key_extra){TK_ROWS(touch_beeb_extra);}else{TK_ROWS(touch_beeb_main);}
    }else{
        if(touch_key_extra){TK_ROWS(touch_master_extra);}else{TK_ROWS(touch_master_main);}
    }
#undef TK_ROWS
    touch_key_count=0;char title[40];snprintf(title,sizeof title,"%s KEYBOARD",machine_name());centred_text(fb,6,title,true);
    touch_key_button(fb,12,19,222,16,TK_MAIN);touch_key_button(fb,246,19,222,16,TK_EXTRA);
    unsigned start=39;
    unsigned height=(208-start)/count;
    for(unsigned r=0;r<count;r++)for(unsigned c=0;c<rows[r].count;c++) {
        unsigned left=8+c*464/rows[r].count,right=8+(c+1)*464/rows[r].count;
        touch_key_button(fb,left,start+r*height,right-left-2,height-2,rows[r].codes[c]);
    }
    touch_key_button(fb,8,214,148,24,TK_RESET);touch_key_button(fb,166,214,148,24,TK_SHIFT);touch_key_button(fb,324,214,148,24,TK_SETTINGS);
}
static void touch_key_activate(int code) {
    if(code==TK_SETTINGS){touch_keyboard=false;fire_armed=false;chooser_dirty=true;return;}
    if(code==TK_SHIFT){touch_key_shift=!touch_key_shift;chooser_dirty=true;return;}
    if(code==TK_SYMBOL){touch_key_symbol=!touch_key_symbol;chooser_dirty=true;return;}
    if(code==TK_MAIN||code==TK_EXTRA){touch_key_extra=code==TK_EXTRA;touch_key_focus=touch_key_extra?1:0;touch_key_count=0;touch_key_armed=false;chooser_dirty=true;return;}
    char command[64];
    if(code==TK_RESET||code==128)snprintf(command,sizeof command,"b");
    else {
        unsigned shift=
            0;
        if(touch_key_shift&&touch_key_symbol)snprintf(command,sizeof command,"d%x d24 d%x u%x u24 u%x",shift,code,code,shift);
        else if(touch_key_shift)snprintf(command,sizeof command,"d%x d%x u%x u%x",shift,code,code,shift);
        else if(touch_key_symbol)snprintf(command,sizeof command,"d24 d%x u%x u24",code,code);
        else snprintf(command,sizeof command,"d%x u%x",code,code);
    }
    // Resume before queuing through the web key sequencer, which holds keys for
    // emulated frames and preserves other input owners. No press occurs paused.
    status_close();
    api_req_t request={.args=command,.from="touchscreen keyboard",.json=true};api_call("keys",&request);
}
static void touch_key_frame(bool tap,unsigned x,unsigned y,uint32_t now) {
    if(tap)for(unsigned i=0;i<touch_key_count;i++) {
        touch_key_button_t b=touch_key_buttons[i];
        if(x>=(unsigned)b.x&&x<(unsigned)(b.x+b.w)&&y>=(unsigned)b.y&&y<(unsigned)(b.y+b.h)) {
            touch_key_focus=i;touch_key_activate(b.code);return;
        }
    }
    unsigned d=0;bool fire=false;joystick_state(&d,&fire);
    if(!fire)touch_key_armed=true;
    int direction=d&1?1:d&2?2:d&4?3:d&8?4:0;
    if(direction&&(direction!=touch_key_direction||(int32_t)(now-touch_key_repeat)>=0)&&touch_key_count) {
        touch_key_button_t from=touch_key_buttons[touch_key_focus];int fx=from.x+from.w/2,fy=from.y+from.h/2;
        int best=100000;unsigned index=touch_key_focus;
        for(unsigned i=0;i<touch_key_count;i++) {
            touch_key_button_t to=touch_key_buttons[i];int dx=to.x+to.w/2-fx,dy=to.y+to.h/2-fy;
            bool valid=direction==1?dy<0:direction==2?dy>0:direction==3?(dx<0&&abs(dy)<8):(dx>0&&abs(dy)<8);
            int score=direction<=2?abs(dy)*100+abs(dx):abs(dx);
            if(valid&&score<best){best=score;index=i;}
        }
        touch_key_focus=index;touch_key_repeat=now+(direction!=touch_key_direction?400:140);chooser_dirty=true;
    }
    touch_key_direction=direction;
    if(fire&&touch_key_armed&&touch_key_count){touch_key_armed=false;touch_key_activate(touch_key_buttons[touch_key_focus].code);}
}

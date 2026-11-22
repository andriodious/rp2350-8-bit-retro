// Paused touchscreen Bluetooth setup. All mutations share the web command path.
// No report counters, raw input or diagnostic exports are presented here.
#if NIMBUS_TEST && BT_KEYBOARD_CLASSIC
#include "boot_setup_keys.h"
enum setup_page { SETUP_NONE, SETUP_HOME, SETUP_SCAN, SETUP_CONTROLS, SETUP_JOY_MAP,
    SETUP_KEY_MAP, SETUP_CAPTURE, SETUP_TARGET, SETUP_FORGET };
enum setup_action { SA_BACK=1, SA_STATUS, SA_EXIT, SA_SCAN, SA_STOP, SA_DISCONNECT,
    SA_RECONNECT, SA_FORGET, SA_CONFIRM, SA_CONTROLS, SA_MAPS, SA_DEVICE, SA_CONNECT,
    SA_PREV, SA_NEXT, SA_MODE, SA_PORT, SA_SOURCE, SA_IX, SA_IY, SA_DEFAULTS, SA_APPLY,
    SA_DEAD, SA_JOY_KEY, SA_MAP_ROW, SA_ADD, SA_DELETE, SA_TARGET };
typedef struct { int16_t x,y,w,h,action,arg; } setup_button_t;
static setup_button_t setup_buttons[24];
static unsigned setup_button_count,setup_focus,setup_offset,setup_selected,setup_usage;
static enum setup_page setup_page;
static bool setup_keyboard,setup_fire_armed,setup_capture_owned;
static int setup_direction;
static uint32_t setup_repeat;
static int32_t setup_config[19];
static char setup_address[18];
static const char *setup_message;
static const char *const setup_inputs[]={"UP","DOWN","LEFT","RIGHT","A","B","X","Y",
    "LEFT SHOULDER","RIGHT SHOULDER","LEFT TRIGGER","RIGHT TRIGGER","MENU"};
static void setup_cancel_capture(void) { if(setup_capture_owned)presto_bluetooth_command("keyboard capture-cancel");setup_capture_owned=false; }
static void setup_go(enum setup_page page) {
    setup_page=page;setup_offset=0;setup_focus=0;setup_button_count=0;
    setup_direction=0;setup_fire_armed=false;setup_message=NULL;chooser_dirty=true;
}
static void setup_open(bool keyboard) {
    setup_keyboard=keyboard;setup_selected=0;setup_address[0]=0;
    controller_get_config(setup_config);setup_go(SETUP_HOME);
}
static void setup_end(void) {
    setup_cancel_capture();
    setup_go(SETUP_NONE);
}
static bool setup_command(const char *command) {
    int result=presto_bluetooth_command(command);
    setup_message=result==200?"DONE":result==404?"NOT FOUND":result==409?"BUSY - DISCONNECT OR STOP SCAN":
        result==503?"WAIT FOR WIFI AND BLUETOOTH":"COULD NOT SAVE OR CONNECT";
    chooser_dirty=true;return result==200;
}
static void setup_save(void) {
    char command[160];size_t n=7;memcpy(command,"config ",7);
    for(unsigned i=0;i<19;i++)n+=snprintf(command+n,sizeof command-n,"%ld%s",(long)setup_config[i],i==18?"":" ");
    setup_command(command);
}
static void setup_defaults(void) {
    const int dirs[4]={
#if MACHINE_C64
        71,7,66,2
#elif MACHINE_SPECTRUM
        10,5,26,25
#else
        57,41,25,121
#endif
    };
    setup_config[0]=1;
#if MACHINE_C64
    setup_config[1]=2;
#else
    setup_config[1]=1;
#endif
    setup_config[2]=0;setup_config[3]=15;
    setup_config[4]=setup_config[5]=0;
    for(unsigned i=0;i<13;i++)setup_config[i+6]=i<4?dirs[i]:i==4?-2:-1;
}
static const char *setup_key_name(int code) {
    if(code==-1)return "NONE";
    if(code==-2)return "FIRE";
    for(unsigned i=0;i<SETUP_KEY_COUNT;i++)if(setup_keys[i].code==code)return setup_keys[i].name;
    return "UNKNOWN KEY";
}
static void setup_usage_name(unsigned usage,char *out,size_t size) {
    if(usage>=4&&usage<=29)snprintf(out,size,"%c",'A'+usage-4);
    else if(usage>=30&&usage<=39)snprintf(out,size,"%u",(usage-29)%10);
    else if(usage>=58&&usage<=69)snprintf(out,size,"F%u",usage-57);
    else {const char *name=usage==40?"RETURN":usage==41?"ESCAPE":usage==42?"BACKSPACE":usage==43?"TAB":usage==44?"SPACE":
        usage==79?"RIGHT":usage==80?"LEFT":usage==81?"DOWN":usage==82?"UP":usage==225?"L SHIFT":usage==229?"R SHIFT":NULL;
        if(name)snprintf(out,size,"%s",name);else snprintf(out,size,"HID %u",usage);}
}
static void setup_button(uint32_t *fb,int x,int y,int w,int h,const char *label,int action,int arg,bool selected) {
    unsigned index=setup_button_count++;
    if(index>=sizeof setup_buttons/sizeof *setup_buttons){setup_button_count--;return;}
    setup_buttons[index]=(setup_button_t){x,y,w,h,action,arg};
    ui_button(fb,x,y,w,h,label,action==SA_EXIT?4:selected?3:0);
    // Focus outline is distinct from the selected device or saved mapping.
    if(index==setup_focus) {
        for(int xx=x+3;xx<x+w-3;xx++){pixel(fb,xx,y+2,3);pixel(fb,xx,y+h-3,3);}
    }
}
static unsigned setup_map_slots(uint8_t map[64],uint8_t slots[32]) {
    controller_keyboard_map_get(map);unsigned n=0;
    for(unsigned i=0;i<64;i+=2)if(map[i])slots[n++]=i;
    return n;
}
static void setup_draw(uint32_t *fb) {
    setup_button_count=0;char line[64];bluetooth_setup_state_t state;
    presto_bluetooth_setup_state(setup_keyboard,&state);
    const char *heading=setup_page==SETUP_HOME?(setup_keyboard?"KEYBOARD SETUP":"JOYSTICK SETUP"):
        setup_page==SETUP_SCAN?"CHOOSE A DEVICE":setup_page==SETUP_CONTROLS?"GAME CONTROLS":
        setup_page==SETUP_JOY_MAP?"BUTTON MAPPINGS":setup_page==SETUP_KEY_MAP?"KEY MAPPINGS":
        setup_page==SETUP_CAPTURE?"PRESS A KEY":setup_page==SETUP_TARGET?"CHOOSE EMULATOR KEY":"FORGET DEVICE";
    centred_text(fb,8,setup_message?setup_message:heading,!setup_message);
    if(setup_page==SETUP_HOME) {
        const char *status=setup_keyboard?presto_keyboard_boot_status():presto_bluetooth_boot_status();
        if(state.scanning)status="SCANNING...";
        else if(state.remembered&&!state.reconnect&&!state.connected&&!state.connecting)status="DISCONNECTED";
        centred_text(fb,28,state.pin[0]?"TYPE PIN THEN RETURN":status,false);
        centred_text(fb,41,state.pin[0]?state.pin:state.address,false);
        centred_text(fb,54,state.remembered?"PAIRING REMEMBERED":"PAIRING NOT SAVED",false);
        setup_button(fb,12,73,222,25,"SCAN",SA_SCAN,0,false);
        setup_button(fb,246,73,222,25,"DISCONNECT",SA_DISCONNECT,0,false);
        setup_button(fb,12,105,222,25,"RECONNECT",SA_RECONNECT,0,false);
        setup_button(fb,246,105,222,25,"FORGET",SA_FORGET,0,false);
        if(!setup_keyboard)setup_button(fb,12,137,456,25,"GAME CONTROLS",SA_CONTROLS,0,false);
        setup_button(fb,12,169,456,25,setup_keyboard?"KEY MAPPINGS":"BUTTON MAPPINGS",SA_MAPS,0,false);
    } else if(setup_page==SETUP_SCAN) {
        centred_text(fb,24,state.scanning?"SCANNING...":"CHOOSE THEN CONNECT",false);
        if(setup_offset>=state.devices)setup_offset=0;
        for(unsigned i=setup_offset;i<state.devices&&i<setup_offset+3;i++) {
            bluetooth_setup_device_t device;if(!presto_bluetooth_setup_device(setup_keyboard,i,&device))continue;
            int y=38+(i-setup_offset)*36;
            snprintf(line,sizeof line,"%.34s",device.name[0]?device.name:"UNNAMED DEVICE");
            setup_button(fb,12,y,456,32,"",SA_DEVICE,i,!strcmp(device.address,setup_address));
            text_ink=!strcmp(device.address,setup_address)?0:7;
            centred_text(fb,y+6,line,false);centred_text(fb,y+19,device.address,false);text_ink=7;
        }
        if(!state.devices)centred_text(fb,79,"PRESS DEVICE CONNECT BUTTON",false);
        setup_button(fb,12,150,140,23,"PREV PAGE",SA_PREV,3,false);
        setup_button(fb,170,150,140,23,"NEXT PAGE",SA_NEXT,3,false);
        setup_button(fb,328,150,140,23,state.scanning?"STOP SCAN":"SCAN",state.scanning?SA_STOP:SA_SCAN,0,false);
        setup_button(fb,12,179,456,23,"CONNECT",SA_CONNECT,0,false);
    } else if(setup_page==SETUP_CONTROLS) {
        const char *modes[]={"OFF","JOYSTICK","KEYBOARD"},*sources[]={"LEFT STICK AND DPAD","RIGHT STICK AND DPAD","DPAD ONLY"};
        snprintf(line,sizeof line,"MODE: %s",modes[setup_config[0]]);setup_button(fb,12,29,456,23,line,SA_MODE,0,false);
        snprintf(line,sizeof line,"PORT: %ld",(long)setup_config[1]);setup_button(fb,12,57,456,23,line,SA_PORT,0,false);
        setup_button(fb,12,85,456,23,sources[setup_config[2]],SA_SOURCE,0,false);
        setup_button(fb,12,113,222,23,setup_config[4]?"INVERT X ON":"INVERT X OFF",SA_IX,0,false);
        setup_button(fb,246,113,222,23,setup_config[5]?"INVERT Y ON":"INVERT Y OFF",SA_IY,0,false);
        snprintf(line,sizeof line,"DEAD ZONE %ld",(long)setup_config[3]);centred_text(fb,145,line,false);
        setup_button(fb,12,139,80,19,"LESS",SA_DEAD,-5,false);setup_button(fb,388,139,80,19,"MORE",SA_DEAD,5,false);slider(fb,165,setup_config[3]*2);
        setup_button(fb,12,182,222,23,"DEFAULTS",SA_DEFAULTS,0,false);setup_button(fb,246,182,222,23,"APPLY",SA_APPLY,0,false);
    } else if(setup_page==SETUP_JOY_MAP||setup_page==SETUP_KEY_MAP) {
        uint8_t map[64],slots[32];unsigned total=setup_page==SETUP_JOY_MAP?13:setup_map_slots(map,slots);
        if(setup_offset>=total)setup_offset=0;
        for(unsigned i=setup_offset;i<total&&i<setup_offset+4;i++) {
            int y=28+(i-setup_offset)*31;
            unsigned slot=setup_page==SETUP_KEY_MAP?slots[i]:i;
            setup_button(fb,12,y,456,28,"",setup_page==SETUP_JOY_MAP?SA_JOY_KEY:SA_MAP_ROW,slot,setup_page==SETUP_KEY_MAP&&setup_selected==slot);
            text_ink=setup_page==SETUP_KEY_MAP&&setup_selected==slot?0:7;
            if(setup_page==SETUP_JOY_MAP)snprintf(line,sizeof line,"%s",setup_inputs[i]);else setup_usage_name(map[slot],line,sizeof line);
            centred_text(fb,y+4,line,false);centred_text(fb,y+17,setup_key_name(setup_page==SETUP_JOY_MAP?setup_config[i+6]:map[slot+1]),false);text_ink=7;
        }
        if(!total)centred_text(fb,76,"NO CUSTOM MAPPINGS",false);
        setup_button(fb,12,156,222,22,"PREV PAGE",SA_PREV,4,false);setup_button(fb,246,156,222,22,"NEXT PAGE",SA_NEXT,4,false);
        setup_button(fb,12,183,222,22,setup_keyboard?"ADD MAPPING":"DEFAULTS",setup_keyboard?SA_ADD:SA_DEFAULTS,0,false);
        setup_button(fb,246,183,222,22,setup_keyboard?"DELETE MAPPING":"APPLY",setup_keyboard?SA_DELETE:SA_APPLY,0,false);
    } else if(setup_page==SETUP_CAPTURE) {
        centred_text(fb,60,"PRESS THE BLUETOOTH KEY",false);centred_text(fb,80,"YOU WANT TO MAP",false);
        centred_text(fb,117,"BACK CANCELS",false);centred_text(fb,137,"WAITING UP TO 60 SECONDS",false);
    } else if(setup_page==SETUP_TARGET) {
        unsigned extra=setup_keyboard?0:2,total=SETUP_KEY_COUNT+extra;
        for(unsigned i=setup_offset;i<total&&i<setup_offset+12;i++) {
            int code=i<extra?(i==0?-1:-2):setup_keys[i-extra].code;
            setup_button(fb,12+((i-setup_offset)%2)*234,27+((i-setup_offset)/2)*25,222,22,setup_key_name(code),SA_TARGET,code,false);
        }
        setup_button(fb,12,182,222,23,"PREV PAGE",SA_PREV,12,false);setup_button(fb,246,182,222,23,"NEXT PAGE",SA_NEXT,12,false);
    } else if(setup_page==SETUP_FORGET) {
        centred_text(fb,65,"REMOVE THE SAVED PAIRING",false);centred_text(fb,87,"FOR THIS DEVICE",false);
        setup_button(fb,12,133,456,30,"FORGET DEVICE",SA_CONFIRM,0,false);
    }
    setup_button(fb,8,214,148,24,"BACK",SA_BACK,0,false);
    setup_button(fb,166,214,148,24,"STATUS",SA_STATUS,0,false);
    setup_button(fb,324,214,148,24,"EXIT",SA_EXIT,0,false);
    if(setup_focus>=setup_button_count)setup_focus=0;
}
static void status_close(void);
static void setup_activate(int action,int arg) {
    char command[64];bluetooth_setup_state_t state;presto_bluetooth_setup_state(setup_keyboard,&state);
    setup_message=NULL;
    switch(action) {
    case SA_EXIT:status_close();return;
    case SA_STATUS:setup_end();return;
    case SA_BACK:
        if(setup_page==SETUP_HOME){setup_end();return;}
        if(setup_page==SETUP_TARGET||setup_page==SETUP_CAPTURE){setup_cancel_capture();setup_go(setup_keyboard?SETUP_KEY_MAP:SETUP_JOY_MAP);}
        else setup_go(SETUP_HOME);
        return;
    case SA_SCAN:
        if(setup_command(setup_keyboard?"keyboard-scan":"scan")){setup_go(SETUP_SCAN);setup_address[0]=0;}return;
    case SA_STOP:setup_command(setup_keyboard?"keyboard-stop":"stop-scan");return;
    case SA_DISCONNECT:setup_command(setup_keyboard?"keyboard disconnect":"disconnect");return;
    case SA_RECONNECT:setup_command(setup_keyboard?"keyboard reconnect":"reconnect");return;
    case SA_FORGET:setup_go(SETUP_FORGET);return;
    case SA_CONFIRM:if(setup_command(setup_keyboard?"keyboard forget":"forget"))setup_go(SETUP_HOME);return;
    case SA_CONTROLS:controller_get_config(setup_config);setup_go(SETUP_CONTROLS);return;
    case SA_MAPS:controller_get_config(setup_config);setup_go(setup_keyboard?SETUP_KEY_MAP:SETUP_JOY_MAP);return;
    case SA_DEVICE:{bluetooth_setup_device_t d;if(presto_bluetooth_setup_device(setup_keyboard,arg,&d)){snprintf(setup_address,sizeof setup_address,"%s",d.address);setup_selected=arg;}break;}
    case SA_CONNECT:{
        if(state.scanning){setup_message="STOP SCAN BEFORE CONNECTING";break;}
        bluetooth_setup_device_t d;
        if(!setup_address[0]||!presto_bluetooth_setup_device(setup_keyboard,setup_selected,&d)||strcmp(d.address,setup_address)) {setup_message="CHOOSE A DEVICE FIRST";break;}
        if(setup_keyboard)snprintf(command,sizeof command,"keyboard connect %s",setup_address);else snprintf(command,sizeof command,"connect %u",setup_selected);
        if(setup_command(command))setup_go(SETUP_HOME);
        return;}
    case SA_PREV:setup_offset=setup_offset>=(unsigned)arg?setup_offset-arg:0;setup_focus=0;break;
    case SA_NEXT:{
        uint8_t map[64],slots[32];unsigned total=setup_page==SETUP_SCAN?state.devices:setup_page==SETUP_JOY_MAP?13:
            setup_page==SETUP_KEY_MAP?setup_map_slots(map,slots):SETUP_KEY_COUNT+(setup_keyboard?0:2);
        if(setup_offset+(unsigned)arg<total)setup_offset+=arg;
        setup_focus=0;break;}
    case SA_DEAD:setup_config[3]+=arg;if(setup_config[3]<0)setup_config[3]=0;if(setup_config[3]>50)setup_config[3]=50;break;
    case SA_MODE:setup_config[0]=(setup_config[0]+1)%3;break;
    case SA_PORT:setup_config[1]=3-setup_config[1];break;
    case SA_SOURCE:setup_config[2]=(setup_config[2]+1)%3;break;
    case SA_IX:setup_config[4]^=1;break;
    case SA_IY:setup_config[5]^=1;break;
    case SA_DEFAULTS:setup_defaults();setup_message="DEFAULTS - PRESS APPLY";break;
    case SA_APPLY:setup_save();return;
    case SA_JOY_KEY:setup_selected=arg;setup_go(SETUP_TARGET);return;
    case SA_MAP_ROW:setup_selected=arg;break;
    case SA_ADD:if(setup_command("keyboard capture")){setup_capture_owned=true;setup_go(SETUP_CAPTURE);}return;
    case SA_DELETE:{uint8_t map[64];controller_keyboard_map_get(map);if(setup_selected<64&&map[setup_selected]){snprintf(command,sizeof command,"keyboard unmap %u",map[setup_selected]);setup_command(command);}else setup_message="CHOOSE A MAPPING FIRST";break;}
    case SA_TARGET:
        if(setup_keyboard){snprintf(command,sizeof command,"keyboard map %u %d",setup_usage,arg);if(setup_command(command))setup_go(SETUP_KEY_MAP);}
        else {setup_config[setup_selected+6]=arg;setup_go(SETUP_JOY_MAP);setup_message="MAPPING SET - PRESS APPLY";}return;
    }
    chooser_dirty=true;
}
static void setup_frame(bool tap,bool touched,unsigned x,unsigned y,uint32_t now) {
    if(setup_page==SETUP_CAPTURE) {
        bluetooth_setup_state_t state;presto_bluetooth_setup_state(true,&state);
        if(state.captured_usage){setup_usage=state.captured_usage;setup_cancel_capture();setup_go(SETUP_TARGET);}
        else if(!state.capturing){setup_cancel_capture();setup_go(SETUP_KEY_MAP);setup_message="CAPTURE ENDED - TRY AGAIN";}
    }
    if(touched&&setup_page==SETUP_CONTROLS&&y>=153&&y<178) {
        int value=x<=20?0:x>=460?50:(x-20)*50/440;
        if(value!=setup_config[3]){setup_config[3]=value;chooser_dirty=true;}
    }
    if(tap)for(unsigned i=0;i<setup_button_count;i++) {
        setup_button_t b=setup_buttons[i];
        if(x>=(unsigned)b.x&&x<(unsigned)(b.x+b.w)&&y>=(unsigned)b.y&&y<(unsigned)(b.y+b.h)) {
            setup_focus=i;setup_activate(b.action,b.arg);return;
        }
    }
    unsigned directions=0;bool fire=false;joystick_state(&directions,&fire);
    if(!fire)setup_fire_armed=true;
    int direction=(directions&1)?-1:(directions&2)?1:0;
    if(direction&&(direction!=setup_direction||(int32_t)(now-setup_repeat)>=0)&&setup_button_count) {
        setup_focus=(setup_focus+setup_button_count+direction)%setup_button_count;
        setup_repeat=now+(direction!=setup_direction?400:140);chooser_dirty=true;
    }
    setup_direction=direction;
    if(fire&&setup_fire_armed&&setup_button_count) {
        setup_button_t b=setup_buttons[setup_focus];setup_fire_armed=false;setup_activate(b.action,b.arg);
    }
}
#else
static void setup_end(void) {}
#endif

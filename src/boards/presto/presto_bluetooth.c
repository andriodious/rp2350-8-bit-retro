/* SDK init hook adapted from Raspberry Pi Pico SDK btstack_cyw43.c.
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* Experimental single-controller Classic HID host with project-managed persistence. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"
#include "classic/btstack_link_key_db_memory.h"
#include "pico/btstack_hci_transport_cyw43.h"
#include "pico/btstack_run_loop_async_context.h"
#include "pico/btstack_cyw43.h"
#include "presto_bluetooth.h"
#include "controller.h"
#include "controller_store.h"
#include "nimbus_reconnect.h"
#include "machine_if.h"
#include "net_if.h"
#include "pico/time.h"

static btstack_link_key_db_t persistent_db;
static const btstack_link_key_db_t *ram_db;
static bool store_error, store_started, reconnect=true;
static uint32_t retry_at;
static unsigned retry_streak;
static unsigned attempts,incoming_count,failures,last_failure,last_failure_phase;
static uint32_t last_failure_ms;
static unsigned startup_events,startup_states,startup_commands,last_startup_event,last_opcode;
static uint32_t startup_ms,last_event_ms;
static int power_result;
static bool start_called;
static uint32_t now_ms(void){return to_ms_since_boot(get_absolute_time());}
static void note_failure(unsigned code,unsigned phase) {
    if(!code)return;
    failures++;last_failure=code;last_failure_phase=phase;last_failure_ms=now_ms();
}
static unsigned machine_index(void) {
    const char *m=machine_name();if(!strcmp(m,"spectrum"))return 3;return !strcmp(m,"master")?0:!strcmp(m,"beeb")?1:2;
}
static bool remembered(bd_addr_t address) {
    link_key_t key;uint8_t type;return controller_store_bond(address,key,&type);
}
static void save_key(bd_addr_t address,link_key_t key,link_key_type_t type) {
    if(!reconnect)return; // Ignore a late pairing result after Disconnect/Forget.
    ram_db->put_link_key(address,key,type);
    controller_store_set_bond(address,key,(uint8_t)type);
}
static void delete_key(bd_addr_t address) {
    bd_addr_t saved;
    ram_db->delete_link_key(address);
    if(remembered(saved)&&!memcmp(saved,address,6))controller_store_forget();
}
/* Supply the SDK init hook, deliberately without the SDK flash TLV backend. */
bool btstack_cyw43_init(async_context_t *context) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(context));
    hci_init(hci_transport_cyw43_instance(), NULL);
    controller_store_init();store_started=true;
    ram_db=btstack_link_key_db_memory_instance();persistent_db=*ram_db;
    persistent_db.put_link_key=save_key;persistent_db.delete_link_key=delete_key;
    hci_set_link_key_db(&persistent_db);
    bd_addr_t address;link_key_t key;uint8_t type;
    if(controller_store_bond(address,key,&type))ram_db->put_link_key(address,key,(link_key_type_t)type);
    int32_t cfg[19];
    if(controller_store_get_config(machine_index(),cfg)) {
        char text[256];size_t n=0;
        for(int i=0;i<19;i++)n+=snprintf(text+n,sizeof text-n,"%s%ld",i?" ":"",(long)cfg[i]);
        controller_config(text);
    }
    return true;
}
void btstack_cyw43_deinit(async_context_t *context) {
    (void)context;
    hci_power_control(HCI_POWER_OFF);
    hci_close();
    btstack_run_loop_async_context_deinit();
    btstack_run_loop_deinit();
    btstack_memory_deinit();
}
static uint8_t descriptor[1024];
static bd_addr_t devices[8], selected;
static unsigned count, reports, fields;
static char names[8][40];
static uint16_t cid;
static bool ready, scanning, connecting, connected, descriptor_ready;
static unsigned error;
static int32_t axes[4], buttons[8], hat, menu, dpad[4];
static int16_t normalized[4];
static int32_t hat_min;static bool has_hat;
static void publish_input(void) {
    unsigned bits=0;
    for(int i=0;i<4;i++)if(dpad[i])bits|=1u<<(i==2?3:i==3?2:i);
    if(has_hat) {
        int h=hat-hat_min;
        static const unsigned dirs[8]={1,9,8,10,2,6,4,5};
        if(h>=0&&h<8)bits|=dirs[h];
    }
    controller_input(connected&&descriptor_ready,normalized,bits,buttons,menu!=0);
}
static btstack_packet_callback_registration_t registration;
static void clear_input(void) {
    memset(normalized,0,sizeof normalized);has_hat=false;
    memset(axes,0,sizeof axes); memset(buttons,0,sizeof buttons); hat=menu=0; memset(dpad,0,sizeof dpad);
    publish_input();
}
static void report(const uint8_t *data, uint16_t len) {
    if (!descriptor_ready || len < 2 || data[0]!=0xa1) return;
    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, hid_descriptor_storage_get_descriptor_data(cid),
        hid_descriptor_storage_get_descriptor_len(cid), HID_REPORT_TYPE_INPUT, data+1,len-1);
    /* Preserve other report IDs' fields; every reported field overwrites its old value. */
    fields=0;
    while(btstack_hid_parser_has_more(&parser)) {
        uint16_t page,usage; int32_t value;
        /* Reject truncated/oversized fields before the SDK parser reads them. */
        const btstack_hid_usage_item_t *item=&parser.descriptor_usage_item;
        unsigned end=item->bit_pos+item->size+(item->report_id!=HID_REPORT_ID_UNDEFINED?8:0);
        if(!item->size||item->size>31||end>(unsigned)(len-1)*8){clear_input();return;}
        int32_t lo=parser.usage_iterator.global_logical_minimum;
        int32_t hi=parser.usage_iterator.global_logical_maximum;
        btstack_hid_parser_get_field(&parser,&page,&usage,&value); fields++;
        if(page==1) {
            int index=usage==0x30?0:usage==0x31?1:usage==0x32?2:usage==0x35?3:-1;
            if(index>=0)normalized[index]=controller_normalize(value,lo,hi);
        }
        if(page==1) {
            if(usage==0x30) axes[0]=value;
            if(usage==0x31) axes[1]=value;
            if(usage==0x32) axes[2]=value;
            if(usage==0x35) axes[3]=value;
            if(usage==0x39){hat=value;hat_min=lo;has_hat=true;}
            if(usage>=0x90 && usage<=0x93) dpad[usage-0x90]=value;
        }
        if(page==9 && usage>=1 && usage<=8) buttons[usage-1]=value;
        if(page==12 && usage==0x223) menu=value;
    }
    reports++;publish_input();
}
static void event(uint8_t type,uint16_t channel,uint8_t *p,uint16_t size) {
    (void)channel;(void)size;
    if(type!=HCI_EVENT_PACKET)return;
    startup_events++;last_startup_event=hci_event_packet_get_type(p);last_event_ms=now_ms();
    if(last_startup_event==HCI_EVENT_COMMAND_COMPLETE){startup_commands++;last_opcode=hci_event_command_complete_get_command_opcode(p);}
    bd_addr_t addr;
    switch(hci_event_packet_get_type(p)) {
    case BTSTACK_EVENT_STATE:
        startup_states++;
        ready=btstack_event_state_get_state(p)==HCI_STATE_WORKING;
        if(ready){retry_streak=0;retry_at=now_ms()+RECONNECT_LISTEN_MS;}
        else {connected=false;clear_input();}
        break;
    case GAP_EVENT_INQUIRY_RESULT:
        gap_event_inquiry_result_get_bd_addr(p,addr);
        for(unsigned i=0;i<count;i++)if(!memcmp(addr,devices[i],6))return;
        if(count<8) {
            memcpy(devices[count],addr,6); names[count][0]=0;
            if(gap_event_inquiry_result_get_name_available(p)) {
                unsigned n=gap_event_inquiry_result_get_name_len(p);
                if(n>39)n=39;
                const uint8_t *name=gap_event_inquiry_result_get_name(p);
                for(unsigned i=0;i<n;i++)names[count][i]=(name[i]>=32 && name[i]<127 && name[i]!='"' && name[i]!='\\')?name[i]:'_';
                names[count][n]=0;
            }
            count++;
        }
        break;
    case GAP_EVENT_INQUIRY_COMPLETE: scanning=false; break;
    case HCI_EVENT_PIN_CODE_REQUEST:
        hci_event_pin_code_request_get_bd_addr(p,addr);
        if(connecting && !memcmp(addr,selected,6))gap_pin_code_response(addr,"0000");
        else gap_pin_code_negative(addr);
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        hci_event_user_confirmation_request_get_bd_addr(p,addr);
        if(connecting && !memcmp(addr,selected,6))gap_ssp_confirmation_response(addr);
        else gap_ssp_confirmation_negative(addr);
        break;
    case HCI_EVENT_HID_META:
        switch(hci_event_hid_meta_get_subevent_code(p)) {
        case HID_SUBEVENT_INCOMING_CONNECTION: {
            bd_addr_t saved;
            hid_subevent_incoming_connection_get_address(p,addr);
            if(reconnect&&!connected&&!connecting&&remembered(saved)&&!memcmp(saved,addr,6)) {
                memcpy(selected,addr,6);connecting=true;incoming_count++;
                cid=hid_subevent_incoming_connection_get_hid_cid(p);
                hid_host_accept_connection(cid,HID_PROTOCOL_MODE_REPORT);
            } else hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(p));
            break;
        }
        case HID_SUBEVENT_CONNECTION_OPENED:
            error=hid_subevent_connection_opened_get_status(p); connecting=false;
            connected=error==0; descriptor_ready=false;
            if(error){note_failure(error,2);retry_at=nimbus_failed_deadline(&retry_streak,now_ms());}
            else retry_streak=0;
            cid=connected?hid_subevent_connection_opened_get_hid_cid(p):0;
            clear_input();break;
        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
            error=hid_subevent_descriptor_available_get_status(p); descriptor_ready=error==0;note_failure(error,3);break;
        case HID_SUBEVENT_REPORT: report(hid_subevent_report_get_report(p),hid_subevent_report_get_report_len(p));break;
        case HID_SUBEVENT_CONNECTION_CLOSED:
            // A real disconnection starts a fresh quick recovery window. A
            // failed-open cleanup must not erase its backoff deadline.
            if(connected){retry_streak=0;retry_at=now_ms()+RECONNECT_LISTEN_MS;}
            cid=0;connected=connecting=descriptor_ready=false;clear_input();break;
        }
    }
}
void presto_bluetooth_start(void) {
    start_called=true;startup_ms=now_ms();
    l2cap_init(); hid_host_init(descriptor,sizeof descriptor); hid_host_register_packet_handler(event);
    registration.callback=event; hci_add_event_handler(&registration);
    gap_set_local_name("Presto Nimbus test");
    // Experimental: 4096 * 0.625 ms = 2.56 s per outgoing page attempt.
    // Keep retry spacing and incoming acceptance unchanged; test switch recovery.
    gap_set_page_timeout(0x1000);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(0);
    bd_addr_t saved;
    gap_discoverable_control(0); gap_connectable_control(remembered(saved));
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE|LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    power_result=hci_power_control(HCI_POWER_ON);
}
int presto_bluetooth_command(const char *s) {
    if(!strncmp(s,"config ",7)) {
        if(!store_started)return 503;
        int result=controller_config(s+7);if(result!=200)return result;
        int32_t cfg[19];controller_get_config(cfg);controller_store_set_config(machine_index(),cfg);
        store_error=!controller_store_flush();return store_error?500:200;
    }
    if(!ready)return 503;
    if(!strcmp(s,"reconnect")) {
        bd_addr_t saved;if(!remembered(saved))return 404;
        // Repeated clicks must neither restart an attempt nor postpone its deadline.
        retry_at=nimbus_manual_deadline(reconnect,scanning||connecting||connected||cid,
            &retry_streak,now_ms(),retry_at);
        reconnect=true;gap_connectable_control(1);return 200;
    }
    if(!strcmp(s,"forget")) {
        reconnect=false;gap_connectable_control(0);
        if(cid)hid_host_disconnect(cid);
        connected=false;descriptor_ready=false;clear_input();
        bd_addr_t saved;if(remembered(saved))ram_db->delete_link_key(saved);
        controller_store_forget();store_error=!controller_store_flush();return store_error?500:200;
    }
    if(!strcmp(s,"scan")) {
        if(scanning||connecting||connected)return 409;
        reconnect=false;count=0;error=gap_inquiry_start(8);scanning=error==0;return error?500:200;
    }
    if(!strcmp(s,"disconnect")) {
        reconnect=false;gap_connectable_control(0);
        if(scanning)gap_inquiry_stop();
        if(cid)hid_host_disconnect(cid);
        connected=false;descriptor_ready=false;
        clear_input();return 200;
    }
    if(!strncmp(s,"connect ",8)) {
        char *end; long i=strtol(s+8,&end,10);
        if(end==s+8||*end||i<0||(unsigned)i>=count)return 400;
        if(scanning||connecting||connected)return 409;
        reconnect=true;gap_connectable_control(1);
        memcpy(selected,devices[i],6); retry_streak=0;connecting=true;
        attempts++;error=hid_host_connect(selected,HID_PROTOCOL_MODE_REPORT,&cid);
        if(error){connecting=false;cid=0;note_failure(error,1);retry_at=nimbus_failed_deadline(&retry_streak,now_ms());}
        return error?500:200;
    }
    return 400;
}
static void bluetooth_frame(bool save) {
    if(!store_started)return;
    /* Never erase flash from a BTstack callback. Do not repeatedly retry failed writes. */
    if(save&&controller_store_pending()&&!store_error)store_error=!controller_store_flush();
    uint32_t now=now_ms();
    // Wi-Fi joins asynchronously. Do not page an absent controller while the
    // shared radio is scanning/associating or waiting for DHCP. Incoming links
    // remain enabled; no retry deadline or failure count changes while waiting.
    if(!net_boot_has_address())return;
    if(!nimbus_retry_due(ready,reconnect,scanning||connecting||connected||cid,now,retry_at))return;
    if(!remembered(selected))return;
    connecting=true;attempts++;
    error=hid_host_connect(selected,HID_PROTOCOL_MODE_REPORT,&cid);
    if(error){connecting=false;cid=0;note_failure(error,1);retry_at=nimbus_failed_deadline(&retry_streak,now);}
}
void presto_bluetooth_frame(void) { bluetooth_frame(true); }
// Core 1 may not yet be ready for flash lockout during startup. Defer bond writes.
void presto_bluetooth_boot_poll(void) { bluetooth_frame(false); }
const char *presto_bluetooth_boot_status(void) {
    bd_addr_t saved;
    if (connected && descriptor_ready) return "CONNECTED";
    if (connected || connecting) return "CONNECTING";
    if (!store_started) return "NOT STARTED";
    if (!remembered(saved)) return "NOT PAIRED";
    if(ready && reconnect && !net_boot_has_address())return "WAITING FOR WIFI";
    return ready ? "SEARCHING" : "STARTING";
}

void presto_bluetooth_prepare_restart(void) {
    if(!store_started)return;
    reconnect=false;gap_connectable_control(0);
    connected=false;descriptor_ready=false;clear_input();controller_frame();
    if(controller_store_pending())store_error=!controller_store_flush();
    // BTstack closes ACL links before powering down the radio. Poll until OFF.
    hci_power_control(HCI_POWER_OFF);
}
bool presto_bluetooth_restart_done(void) {
    return !store_started||hci_get_state()==HCI_STATE_OFF;
}
void presto_bluetooth_json(char *out,size_t size) {
    bd_addr_t saved;bool bonded=remembered(saved);
    int32_t remaining=(int32_t)(retry_at-now_ms());
    unsigned wait_ms=reconnect&&bonded&&!connecting&&!connected&&remaining>0?(unsigned)remaining:0;
    char config[160];controller_json(config,sizeof config);
    char list[768];size_t used=0;list[0]=0;
    for(unsigned i=0;i<count;i++)used+=snprintf(list+used,sizeof list-used,"%s\"%s %s\"",i?",":"",bd_addr_to_str(devices[i]),names[i]);
    snprintf(out,size,",\"bluetooth\":{\"build\":8,\"page_timeout_ms\":2560,\"wifi_wait\":%s,\"retry_streak\":%u,\"last_failure\":%u,\"last_failure_phase\":%u,\"last_failure_ms\":%lu,\"reconnect_enabled\":%s,\"retry_ms\":%u,\"attempts\":%u,\"incoming\":%u,\"failures\":%u,\"remembered\":%s,\"save_error\":%s,\"save_pending\":%s,\"config\":[%s],\"normalized\":[%d,%d,%d,%d],\"state\":\"%s\",\"error\":%u,\"descriptor\":%s,\"reports\":%u,\"fields\":%u,\"devices\":[%s],\"axes\":[%ld,%ld,%ld,%ld],\"buttons\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],\"hat\":%ld,\"menu\":%ld,\"dpad\":[%ld,%ld,%ld,%ld]}",
        (reconnect&&!connected&&!net_boot_has_address())?"true":"false",retry_streak,last_failure,last_failure_phase,(unsigned long)last_failure_ms,reconnect?"true":"false",wait_ms,attempts,incoming_count,failures,bonded?"true":"false",store_error?"true":"false",controller_store_pending()?"true":"false",config,normalized[0],normalized[1],normalized[2],normalized[3],!ready?"starting":connected?"connected":connecting?"connecting":scanning?"scanning":"idle",error,descriptor_ready?"true":"false",reports,fields,list,
        (long)axes[0],(long)axes[1],(long)axes[2],(long)axes[3],(long)buttons[0],(long)buttons[1],(long)buttons[2],(long)buttons[3],(long)buttons[4],(long)buttons[5],(long)buttons[6],(long)buttons[7],(long)hat,(long)menu,(long)dpad[0],(long)dpad[1],(long)dpad[2],(long)dpad[3]);
#if MACHINE_SPECTRUM
    size_t n=strlen(out);
    if(n<size)snprintf(out+n,size-n,",\"bluetooth_startup\":{\"called\":%s,\"store_started\":%s,\"ready\":%s,\"power_result\":%d,\"hci_state\":%u,\"start_ms\":%lu,\"last_event_ms\":%lu,\"events\":%u,\"state_events\":%u,\"commands\":%u,\"last_event\":%u,\"last_opcode\":%u}",start_called?"true":"false",store_started?"true":"false",ready?"true":"false",power_result,store_started?(unsigned)hci_get_state():0,(unsigned long)startup_ms,(unsigned long)last_event_ms,startup_events,startup_states,startup_commands,last_startup_event,last_opcode);
#endif

}

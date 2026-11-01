/* SDK init hook adapted from Raspberry Pi Pico SDK btstack_cyw43.c.
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* Experimental single-controller Classic HID host. No flash pairing storage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"
#include "classic/btstack_link_key_db_memory.h"
#include "pico/btstack_hci_transport_cyw43.h"
#include "pico/btstack_run_loop_async_context.h"
#include "pico/btstack_cyw43.h"
#include "presto_bluetooth.h"

/* Supply the SDK init hook, deliberately without the SDK flash TLV backend. */
bool btstack_cyw43_init(async_context_t *context) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(context));
    hci_init(hci_transport_cyw43_instance(), NULL);
    hci_set_link_key_db(btstack_link_key_db_memory_instance());
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
static btstack_packet_callback_registration_t registration;
static void clear_input(void) {
    memset(axes,0,sizeof axes); memset(buttons,0,sizeof buttons); hat=menu=0; memset(dpad,0,sizeof dpad);
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
        btstack_hid_parser_get_field(&parser,&page,&usage,&value); fields++;
        if(page==1) {
            if(usage==0x30) axes[0]=value;
            if(usage==0x31) axes[1]=value;
            if(usage==0x32) axes[2]=value;
            if(usage==0x35) axes[3]=value;
            if(usage==0x39) hat=value;
            if(usage>=0x90 && usage<=0x93) dpad[usage-0x90]=value;
        }
        if(page==9 && usage>=1 && usage<=8) buttons[usage-1]=value;
        if(page==12 && usage==0x223) menu=value;
    }
    reports++;
}
static void event(uint8_t type,uint16_t channel,uint8_t *p,uint16_t size) {
    (void)channel;(void)size;
    if(type!=HCI_EVENT_PACKET)return;
    bd_addr_t addr;
    switch(hci_event_packet_get_type(p)) {
    case BTSTACK_EVENT_STATE: ready=btstack_event_state_get_state(p)==HCI_STATE_WORKING; break;
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
        case HID_SUBEVENT_INCOMING_CONNECTION:
            hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(p)); break;
        case HID_SUBEVENT_CONNECTION_OPENED:
            error=hid_subevent_connection_opened_get_status(p); connecting=false;
            connected=error==0; descriptor_ready=false;
            cid=connected?hid_subevent_connection_opened_get_hid_cid(p):0;
            clear_input();break;
        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
            error=hid_subevent_descriptor_available_get_status(p); descriptor_ready=error==0;break;
        case HID_SUBEVENT_REPORT: report(hid_subevent_report_get_report(p),hid_subevent_report_get_report_len(p));break;
        case HID_SUBEVENT_CONNECTION_CLOSED:
            cid=0;connected=connecting=descriptor_ready=false;clear_input();break;
        }
    }
}
void presto_bluetooth_start(void) {
    l2cap_init(); hid_host_init(descriptor,sizeof descriptor); hid_host_register_packet_handler(event);
    registration.callback=event; hci_add_event_handler(&registration);
    gap_set_local_name("Presto Nimbus test");
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(0);
    gap_discoverable_control(0); gap_connectable_control(0);
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE|LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_power_control(HCI_POWER_ON);
}
int presto_bluetooth_command(const char *s) {
    if(!ready)return 503;
    if(!strcmp(s,"scan")) {
        if(scanning||connecting||connected)return 409;
        count=0;error=gap_inquiry_start(8);scanning=error==0;return error?500:200;
    }
    if(!strcmp(s,"disconnect")) {
        if(scanning)gap_inquiry_stop();
        if(cid)hid_host_disconnect(cid);
        clear_input();return 200;
    }
    if(!strncmp(s,"connect ",8)) {
        char *end; long i=strtol(s+8,&end,10);
        if(end==s+8||*end||i<0||(unsigned)i>=count)return 400;
        if(scanning||connecting||connected)return 409;
        memcpy(selected,devices[i],6); connecting=true;
        error=hid_host_connect(selected,HID_PROTOCOL_MODE_REPORT,&cid);
        if(error)connecting=false;
        return error?500:200;
    }
    return 400;
}
void presto_bluetooth_json(char *out,size_t size) {
    char list[768];size_t used=0;list[0]=0;
    for(unsigned i=0;i<count;i++)used+=snprintf(list+used,sizeof list-used,"%s\"%s %s\"",i?",":"",bd_addr_to_str(devices[i]),names[i]);
    snprintf(out,size,",\"bluetooth\":{\"state\":\"%s\",\"error\":%u,\"descriptor\":%s,\"reports\":%u,\"fields\":%u,\"devices\":[%s],\"axes\":[%ld,%ld,%ld,%ld],\"buttons\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],\"hat\":%ld,\"menu\":%ld,\"dpad\":[%ld,%ld,%ld,%ld]}",
        !ready?"starting":connected?"connected":connecting?"connecting":scanning?"scanning":"idle",error,descriptor_ready?"true":"false",reports,fields,list,
        (long)axes[0],(long)axes[1],(long)axes[2],(long)axes[3],(long)buttons[0],(long)buttons[1],(long)buttons[2],(long)buttons[3],(long)buttons[4],(long)buttons[5],(long)buttons[6],(long)buttons[7],(long)hat,(long)menu,(long)dpad[0],(long)dpad[1],(long)dpad[2],(long)dpad[3]);
}

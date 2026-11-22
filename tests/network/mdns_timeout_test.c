#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/init.h"
#include "netif/ppp/ppp_opts.h"
#include "lwip/timeouts.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/mdns_out.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/ip.h"
#include "lwip/prot/dns.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"

static u32_t now;
static unsigned queries, peak, replies;
u32_t sys_now(void) { return now; }
u32_t sys_jiffies(void) { return now; }
sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t p) { (void)p; }
void host_assert(const char *message, const char *file, int line) {
    fprintf(stderr, "ASSERT after %u queries: %s (%s:%d)\n", queries, message, file, line);
    exit(86);
}
static err_t output(struct netif *n, struct pbuf *p, const ip4_addr_t *ip) {
    (void)n; (void)p; (void)ip; replies++; return ERR_OK;
}
static err_t linkout(struct netif *n, struct pbuf *p) { (void)n; (void)p; return ERR_OK; }
static err_t interface_init(struct netif *n) {
    n->hwaddr_len = 6; n->hwaddr[0] = 2; n->hwaddr[5] = 42;
    n->name[0] = 'h'; n->name[1] = '0';
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
    n->output = output; n->linkoutput = linkout; n->mtu = 1500;
    return ERR_OK;
}

/* Use the real UDP receive callback without any network I/O or source changes. */
#include "mdns.c"

static unsigned active_timers(void) {
    unsigned count = 0;
    for (struct sys_timeo *t = *sys_timeouts_get_next_timeout(); t; t = t->next) count++;
    if (count > peak) peak = count;
    return count;
}
static void advance(unsigned ms) {
    for (unsigned i = 0; i < ms; i++) { now++; sys_check_timeouts(); active_timers(); }
}
static void question(struct netif *n, int truncated, int unicast, int shared) {
    /* Standard A/IN question for 8bitretro.local, optionally TC and QU flagged. */
    unsigned char q[] = {0,0,0,0,0,1,0,0,0,0,0,0,9,'8','b','i','t','r','e','t','r','o',
                         5,'l','o','c','a','l',0,0,1,0,1};
    unsigned char ptr[] = {0,0,0,0,0,1,0,0,0,0,0,0,5,'_','h','t','t','p',
                           4,'_','t','c','p',5,'l','o','c','a','l',0,0,12,0,1};
    unsigned char *data = shared ? ptr : q;
    unsigned size = shared ? sizeof(ptr) : sizeof(q);
    if (truncated) data[2] = DNS_FLAG1_TRUNC;
    if (unicast) data[size-2] = 0x80;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_RAM);
    assert(p && pbuf_take(p, data, size) == ERR_OK);
    p->if_idx = netif_get_index(n);
    IP4_ADDR(&ip_data.current_iphdr_src, 192,168,1,100 + queries % 20);
    IP4_ADDR(&ip_data.current_iphdr_dest, 224,0,0,251);
    ip_data.current_netif = n; ip_data.current_input_netif = n;
    queries++;
    mdns_recv(NULL, mdns_pcb, p, &ip_data.current_iphdr_src, 5353);
    active_timers();
}
static void query(struct netif *n, int truncated, int unicast) {
    question(n, truncated, unicast, 0);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int spaced = !strcmp(argv[1], "spaced");
    int lifecycle = !strcmp(argv[1], "lifecycle");
    lwip_init();
    struct netif n = {0}; ip4_addr_t ip, mask, gateway;
    IP4_ADDR(&ip, 192,168,1,42); IP4_ADDR(&mask, 255,255,255,0); IP4_ADDR(&gateway, 192,168,1,1);
    assert(netif_add(&n, &ip, &mask, &gateway, NULL, interface_init, ip4_input));
    netif_set_default(&n); netif_set_up(&n); netif_set_link_up(&n);
    mdns_resp_init(); assert(mdns_resp_add_netif(&n, "8bitretro") == ERR_OK);
    assert(mdns_resp_add_service(&n, "8-bit Retro", "_http", DNSSD_PROTO_TCP, 80, NULL, NULL) >= 0);
    /* An active TCP connection keeps the shared HTTP timer in the budget. */
    struct tcp_pcb *tcp = tcp_new(); assert(tcp);
    tcp->state = ESTABLISHED; TCP_REG(&tcp_active_pcbs, tcp);
    advance(10000);
    const unsigned settled = active_timers();
    if (lifecycle) {
        question(&n, 0, 0, 1); /* Delayed multicast PTR response. */
        query(&n, 0, 0);       /* Immediate A response: three suppression timers. */
        question(&n, 0, 1, 1); /* Delayed unicast PTR response. */
        for (unsigned i = 0; i < MDNS_MAX_STORED_PKTS; i++) query(&n, 1, 0);
        mdns_resp_restart(&n); /* Probe timer overlaps all the above. */
        assert(active_timers() == LWIP_NUM_SYS_TIMEOUT_INTERNAL + MDNS_MAX_STORED_PKTS + 6);
        advance(11000);
        assert(active_timers() == settled);
    }
    const unsigned cycles = lifecycle ? 100 : 1;
    for (unsigned cycle = 0; cycle < cycles; cycle++) {
        unsigned before = replies;
        query(&n, 0, 0); advance(120);
        for (unsigned i = 0; i < MDNS_MAX_STORED_PKTS; i++) {
            query(&n, 1, 0);
            if (spaced) advance(600);
        }
        if (lifecycle) {
            query(&n, 1, 0); /* Full stored-question pool must reject safely. */
            query(&n, 0, 1); /* QU response while multicast timers remain active. */
            mdns_resp_restart(&n); /* Probe/announce alongside pending questions. */
            active_timers();
        }
        advance(11000);
        assert(replies > before);
        assert(active_timers() == settled); /* No timer leak across rounds. */
    }
    printf("PASS %s: %u queries, %u rounds, peak %u/%u timers, settled %u\n",
           argv[1], queries, cycles, peak, MEMP_NUM_SYS_TIMEOUT, settled);
    return 0;
}

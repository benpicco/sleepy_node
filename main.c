#include <stdio.h>

#include "periph/rtc.h"
#include "periph/pm.h"
#include "msg.h"
#include "mutex.h"
#include "net/gcoap.h"
#include "net/gnrc/netif.h"

#define NETIF_NUM_MAX               (2)
#define BORDER_ROUTER_TIMEOUT_US    (2500 * US_PER_MS)

/* address used for the host by gnrc_border_router */
#define COAP_SERVER_PREFIX (0xfdeadbee000f0000ULL)
#define COAP_SERVER_IID    (0x1ULL)


/* ifconfig */
int _gnrc_netif_config(int argc, char **argv);

static void _rtc_alarm(void* arg)
{
    (void) arg;

    pm_reboot();
    return;
}

static void _gnrc_netapi_set_all(netopt_state_t state)
{
    gnrc_netif_t* netif = NULL;
    while ((netif = gnrc_netif_iter(netif))) {
        /* retry if busy */
        while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                               &state, sizeof(state)) == -EBUSY) {}
    }
}

static void pm_hibernate_for(unsigned sec)
{
    struct tm now;

    /* disable all network interfaces */
    _gnrc_netapi_set_all(NETOPT_STATE_SLEEP);

    rtc_get_time(&now);
    now.tm_sec += sec;
    rtc_set_alarm(&now, _rtc_alarm, NULL);
    pm_set(0);
}

struct time_ctx {
    mutex_t lock;
    int code;
};

static void _time_resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t* pdu,
                               const sock_udp_ep_t *remote)
{
    (void)remote;       /* not interested in the source currently */

    struct time_ctx *ctx = memo->context;

    if (memo->state == GCOAP_MEMO_TIMEOUT) {
        puts("timeout");
        ctx->code = -ETIMEDOUT;
        goto out;
    }
    else if (memo->state == GCOAP_MEMO_ERR) {
        puts("bad message");
        ctx->code = -EBADMSG;
        goto out;
    }

    coap_block1_t block;
    coap_get_block2(pdu, &block);

    if (coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
        puts("invalid payload");
        ctx->code = -EBADMSG;
        goto out;
    }

    puts((char*)pdu->payload);

    ctx->code = 0;

out:
    mutex_unlock(&ctx->lock);
}

static void _get_addr(ipv6_addr_t *addr)
{
    addr->u64[0] = byteorder_htonll(COAP_SERVER_PREFIX);
    addr->u64[1] = byteorder_htonll(COAP_SERVER_IID);
}

static int coap_get_time(void)
{
    size_t len;
    coap_pkt_t pdu;
    uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
    sock_udp_ep_t remote;

    struct time_ctx ctx = {
        .lock = MUTEX_INIT_LOCKED,
    };

    gcoap_req_init(&pdu, &buf[0], CONFIG_GCOAP_PDU_BUF_SIZE, COAP_METHOD_GET, "/time");

    /* set non-confirmable */
    coap_hdr_set_type(pdu.hdr, COAP_TYPE_NON);

    /* options done */
    len = coap_opt_finish(&pdu, COAP_OPT_FINISH_NONE);

    /* fill in CoAP server information */
    remote.family = AF_INET6;
    remote.netif  = SOCK_ADDR_ANY_NETIF;
    remote.port   = COAP_PORT;

    _get_addr((ipv6_addr_t *) &remote.addr);

    /* send COAP request */
    gcoap_req_send(buf, len, &remote, _time_resp_handler, &ctx);

    /* wait for response */
    mutex_lock(&ctx.lock);

    return ctx.code;
}


int main(void)
{
    msg_t _main_msg_queue[4];
    msg_bus_entry_t subs[NETIF_NUM_MAX];
    unsigned count;
    gnrc_netif_t *netif;

    msg_init_queue(_main_msg_queue, ARRAY_SIZE(_main_msg_queue));

    /* enable network interfaces */
    gnrc_netif_init_devs();

    /* subscribe to all interfaces */
    count = 0;
    netif = NULL;
    while ((netif = gnrc_netif_iter(netif)) && count < NETIF_NUM_MAX) {
        msg_bus_t *bus = gnrc_netif_get_bus(netif, GNRC_NETIF_BUS_IPV6);

        msg_bus_attach(bus, &subs[count]);
        msg_bus_subscribe(&subs[count], GNRC_IPV6_EVENT_ADDR_VALID);

        ++count;
    }

    /* wait for global address */
    msg_t m;
    do {
        if (xtimer_msg_receive_timeout(&m, BORDER_ROUTER_TIMEOUT_US) < 0) {
            puts(">> border router timeout <<");
            _gnrc_netif_config(0, NULL);
            goto out;
        }
    } while (ipv6_addr_is_link_local(m.content.ptr));

    if (coap_get_time()) {
        puts(">> get time failed <<");
    }

out:
    /* unsubscribe */
    count = 0;
    netif = NULL;
    while ((netif = gnrc_netif_iter(netif)) && count < NETIF_NUM_MAX) {
        msg_bus_t *bus = gnrc_netif_get_bus(netif, GNRC_NETIF_BUS_IPV6);
        msg_bus_detach(bus, &subs[count++]);
    }

    pm_hibernate_for(5);
    puts(">> hibernate failed? <<");

    return 0;
}

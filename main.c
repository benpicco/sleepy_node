#include <stdio.h>

#include "periph/rtc.h"
#include "periph/pm.h"
#include "msg.h"
#include "net/gnrc/netif.h"

#define NETIF_NUM_MAX               (2)
#define BORDER_ROUTER_TIMEOUT_US    (1000 * US_PER_MS)

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
            break;
        }
    } while (ipv6_addr_is_link_local(m.content.ptr));

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

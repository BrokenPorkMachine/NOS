#include "netstack.h"
#include "e1000.h"
#include "../IO/serial.h"
#include <stdint.h>
#include <string.h>

// Simple loopback ring buffer so the network servers can exchange data.
#define NETBUF_SIZE 2048
static uint8_t netbuf[NETBUF_SIZE];
static size_t head = 0;
static size_t tail = 0;

static size_t netbuf_avail(void) {
    if (head >= tail)
        return head - tail;
    return NETBUF_SIZE - tail + head;
}

void net_init(void) {
    serial_puts("[net] initializing network stack\n");
    int nic = e1000_init();
    if (nic < 0)
        serial_puts("[net] no supported NIC found\n");
    else
        serial_puts("[net] NIC ready (driver stub)\n");
    head = tail = 0;
}

int net_send(const void *data, size_t len) {
    const uint8_t *d = (const uint8_t *)data;
    size_t space = NETBUF_SIZE - netbuf_avail();
    if (len > space)
        len = space; // drop excess
    for (size_t i = 0; i < len; ++i) {
        netbuf[head++] = d[i];
        if (head >= NETBUF_SIZE)
            head = 0;
    }
    return (int)len;
}

int net_receive(void *buf, size_t buflen) {
    uint8_t *b = (uint8_t *)buf;
    size_t avail = netbuf_avail();
    if (avail == 0)
        return 0;
    if (buflen < avail)
        avail = buflen;
    for (size_t i = 0; i < avail; ++i) {
        b[i] = netbuf[tail++];
        if (tail >= NETBUF_SIZE)
            tail = 0;
    }
    return (int)avail;
}

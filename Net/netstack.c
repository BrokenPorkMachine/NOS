#include "netstack.h"
#include "e1000.h"
#include "../IO/serial.h"
#include <stdint.h>
#include <string.h>

// Simple loopback ring buffers indexed by port.  This allows the demo
// network servers (VNC, SSH, FTP) to operate independently while still
// using the very small in-memory transport.
#define NET_PORTS   8
#define NETBUF_SIZE 512

static uint8_t netbuf[NET_PORTS][NETBUF_SIZE];
static size_t head[NET_PORTS];
static size_t tail[NET_PORTS];

static size_t netbuf_avail(unsigned p) {
    size_t h = head[p];
    size_t t = tail[p];
    if (h >= t)
        return h - t;
    return NETBUF_SIZE - t + h;
}

void net_init(void) {
    serial_puts("[net] initializing network stack\n");
    int nic = e1000_init();
    if (nic < 0)
        serial_puts("[net] no supported NIC found\n");
    else
        serial_puts("[net] NIC ready (driver stub)\n");
    for (unsigned i = 0; i < NET_PORTS; ++i)
        head[i] = tail[i] = 0;
}

int net_send(unsigned port, const void *data, size_t len) {
    if (port >= NET_PORTS)
        return 0;
    const uint8_t *d = (const uint8_t *)data;
    size_t space = NETBUF_SIZE - netbuf_avail(port);
    if (len > space)
        len = space; // drop excess
    for (size_t i = 0; i < len; ++i) {
        netbuf[port][head[port]++] = d[i];
        if (head[port] >= NETBUF_SIZE)
            head[port] = 0;
    }
    return (int)len;
}

int net_receive(unsigned port, void *buf, size_t buflen) {
    if (port >= NET_PORTS)
        return 0;
    uint8_t *b = (uint8_t *)buf;
    size_t avail = netbuf_avail(port);
    if (avail == 0)
        return 0;
    if (buflen < avail)
        avail = buflen;
    for (size_t i = 0; i < avail; ++i) {
        b[i] = netbuf[port][tail[port]++];
        if (tail[port] >= NETBUF_SIZE)
            tail[port] = 0;
    }
    return (int)avail;
}

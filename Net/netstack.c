#include "netstack.h"
#include "e1000.h"
#include "../IO/serial.h"

void net_init(void) {
    serial_puts("[net] initializing network stack\n");
    int nic = e1000_init();
    if (nic < 0)
        serial_puts("[net] no supported NIC found\n");
    else
        serial_puts("[net] NIC ready (driver stub)\n");
}

int net_send(const void *data, size_t len) {
    (void)data; (void)len;
    serial_puts("[net] send not implemented\n");
    return -1;
}

int net_receive(void *buf, size_t buflen) {
    (void)buf; (void)buflen;
    serial_puts("[net] receive not implemented\n");
    return 0;
}

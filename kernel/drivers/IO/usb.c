#include "usb.h"
#include "pci.h"
#include "serial.h"

void usb_init(void) {
    /* Scan PCI bus for USB controllers and initialize them.
       Real hardware handling to be implemented. */
    serial_puts("USB: initialization stub\n");
}

void usb_poll(void) {
    /* Placeholder for polling USB events */
}

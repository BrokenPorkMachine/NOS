#include "usbkbd.h"
#include "serial.h"

void usb_kbd_init(void) {
    // Real USB keyboard handling is not implemented yet, but we
    // announce initialization so higher layers know the driver ran.
    serial_puts("USB keyboard init\n");
}


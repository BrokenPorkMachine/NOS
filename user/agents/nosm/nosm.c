#include "drivers/IO/serial.h"

// Minimal placeholder for the NOSM module manager.
// For now it simply announces itself so the kernel can
// verify that the thread was launched.
void nosm_entry(void) {
    serial_puts("[nosm] module manager initialized\n");
}

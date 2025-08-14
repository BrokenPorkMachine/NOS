#include "drivers/IO/serial.h"

extern int kprintf(const char *fmt, ...);

// Minimal placeholder for the NOSM module manager.
// For now it simply announces itself so the kernel can
// verify that the thread was launched.
void nosm_entry(void) {
    kprintf("[nosm] module manager initialized\n");
    serial_puts("[nosm] module manager initialized\n");
}

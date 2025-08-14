#include "drivers/IO/serial.h"
#include "regx_key.h"

extern int kprintf(const char *fmt, ...);

// Minimal placeholder for the NOSM module manager.
// For now it simply announces itself so the kernel can
// verify that the thread was launched.
void nosm_entry(void) {
    if (regx_verify_launch_key(REGX_LAUNCH_KEY) != 0) {
        kprintf("[nosm] invalid launch key\n");
        return;
    }
    kprintf("[nosm] module manager initialized\n");
    serial_puts("[nosm] module manager initialized\n");
}

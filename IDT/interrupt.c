#include <stdint.h>
#include "../Task/thread.h"
void isr_default_handler(uint64_t *rsp) {
    // You could output to serial here or VGA
    volatile char *vga = (char*)0xB8000 + 160; // Line 2
    const char *msg = "INTERRUPT!";
    for (int i = 0; msg[i]; ++i)
        vga[i*2] = msg[i];
    while (1) __asm__ volatile ("hlt");
}

void isr_timer_handler(void) {
    static int ticks = 0;
    ++ticks;
    // Write a clock value to VGA
    volatile char* vga = (char*)0xB8000 + 160;
    vga[0] = '0' + (ticks % 10);
    schedule();
}

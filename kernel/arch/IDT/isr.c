#include "arch/IDT/isr.h"
#ifndef kprintf
#include "../../klib/stdio.h"
#define kprintf printf
#endif

/* Timer tick counter and simple watchdog countdown. */
static volatile unsigned ticks;
static volatile unsigned init_watchdog;

void arm_init_watchdog(unsigned t) { init_watchdog = t; }

void isr_timer_handler(const void *hw_frame) {
    (void)hw_frame;
    if (++ticks % 100 == 0) kprintf("[timer] %u ticks\n", ticks);

    if (init_watchdog) {
        if (--init_watchdog == 0) {
            kprintf("[watchdog] init thread stalled\n");
            for (;;) __asm__ volatile("hlt");
        }
    }
}

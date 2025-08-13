#include "arch/IDT/isr.h"
#ifndef kprintf
#include <stdio.h>
#define kprintf printf
#endif

static volatile unsigned ticks;

void isr_timer_handler(const void *hw_frame) {
    (void)hw_frame;
    if (++ticks % 100 == 0) kprintf("[timer] %u ticks\n", ticks);
}

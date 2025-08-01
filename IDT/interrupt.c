#include <stdint.h>
#include "../Task/thread.h"
#include "../VM/cow.h"
#include "../IO/serial.h"
void isr_default_handler(uint64_t *rsp) {
    (void)rsp; // unused in default handler
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
    // Preempt current thread on each timer tick
    thread_yield();
}

void isr_page_fault_handler(uint64_t error_code, uint64_t addr) {
    serial_puts("[fault] page fault\n");
    handle_page_fault(error_code, addr);
}

#include <stdint.h>
#include "../IDT/idt.h"
#include "../VM/paging.h"
#include "../Task/thread.h"
#include "../GDT/gdt.h"
#include "../IO/pic.h"
#include "../IO/pit.h"
#include "../IO/keyboard.h"
#include "../IO/mouse.h"
#include "../Net/e1000.h"
#include "syscall.h"

#define VGA_TEXT_BUF 0xB8000

static void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i]; // White on black
        i++;
    }
}

static int log_row = 1;

static void log_line(const char *s)
{
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF + log_row * 80;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i];
        i++;
    }
    if (log_row < 24)
        log_row++;
}

void kernel_main(void) {
    vga_write("Mach Microkernel: Boot OK");
    log_line("[init] IDT");
    idt_install();
    log_line("[init] PIC");
    pic_remap();
    log_line("[init] PIT");
    pit_init(100);
    log_line("[init] Keyboard");
    keyboard_init();
    log_line("[init] Mouse");
    mouse_init();
    log_line("[init] NIC");
    e1000_init();
    log_line("[init] Paging");
    paging_init();
    log_line("[init] Threads");
    threads_init();
    log_line("[init] GDT");
    gdt_install();
    log_line("[init] start first thread");

    // Start first thread
    asm volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        : : "r"(current->rsp), "r"(current->func)
    );

    while (1) __asm__ volatile ("hlt");
}


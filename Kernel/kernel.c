#include <stdint.h>
#include "../IDT/idt.h"
#include "../VM/paging.h"

#define VGA_TEXT_BUF 0xB8000

void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i]; // White on black
        i++;
    }
}

void kernel_main(void) {
    vga_write("Mach Microkernel: Boot OK");
    idt_install();
    paging_init();

    // Paging is now enabled! Try to access RAM and VGA normally.
    while (1) __asm__ volatile ("hlt");
}

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

#define VGA_TEXT_BUF 0xB8000

void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i]; // White on black
        i++;
    }
}

// Setup a minimal handler for int $0x80 (IDT vector 0x80)
// syscall 0 -> cooperative yield
void isr_syscall_handler(void) {
    uint64_t num;
    asm volatile("mov %%rax, %0" : "=r"(num));
    if (num == 0)
        schedule();
}

void kernel_main(void) {
    vga_write("Mach Microkernel: Boot OK");
    idt_install();
    pic_remap();
    pit_init(100);
    keyboard_init();
    mouse_init();
    e1000_init();
    paging_init();
    threads_init();
    gdt_install();

    // Start first thread
    asm volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        : : "r"(current->rsp), "r"(current->func)
    );

    while (1) __asm__ volatile ("hlt");
}


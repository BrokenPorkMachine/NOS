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

void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i]; // White on black
        i++;
    }
}

// System call dispatcher for int $0x80
void isr_syscall_handler(void) {
    uint64_t num, a1, a2, a3, ret;
    asm volatile("mov %%rax, %0" : "=r"(num));
    asm volatile("mov %%rdi, %0" : "=r"(a1));
    asm volatile("mov %%rsi, %0" : "=r"(a2));
    asm volatile("mov %%rdx, %0" : "=r"(a3));
    ret = syscall_handle(num, a1, a2, a3);
    asm volatile("mov %0, %%rax" :: "r"(ret));
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


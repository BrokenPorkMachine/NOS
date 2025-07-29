#include <stdint.h>
#include "../IDT/idt.h"
#include "../VM/paging.h"
#include "../Task/thread.h"
#include "../GDT/gdt.h"
#include "../IO/pic.h"
#include "../IO/pit.h"
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

// Setup a minimal handler for int $0x80 (IDT vector 0x80)
void isr_syscall_handler(void) {
    uint64_t num;
    asm volatile("mov %%rax, %0" : "=r"(num));
    syscall_handle(num);
}

void kernel_main(void) {
    vga_write("Mach Microkernel: Boot OK");
    idt_install();
    pic_remap();
    pit_init(100);
    paging_init();
    threads_init();
    gdt_install();

    // --- Setup IDT, paging, PIT, etc. here ---
    // Install int 0x80 handler in IDT (pointing to isr_syscall_stub, not shown)

    // Start first thread
    asm volatile(
        "mov %0, %%rsp\n"
        "call *%1\n"
        : : "r"(current->rsp), "r"(current->func)
    );

    while (1) __asm__ volatile ("hlt");
}


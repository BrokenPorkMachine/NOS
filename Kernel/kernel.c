#include <stdint.h>
#include "bootinfo.h"
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
#define VGA_COLS 80
#define VGA_ROWS 25

static int log_row = 1;

static void vga_clear() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (0x0F << 8) | ' ';
    log_row = 1;
}

static void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i] && i < VGA_COLS) {
        vga[i] = (0x0F << 8) | s[i];
        i++;
    }
}

static void log_line(const char *s)
{
    if (log_row >= VGA_ROWS-1) log_row = 1; // (simple scroll)
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF + log_row * VGA_COLS;
    int i = 0;
    while (s[i] && i < VGA_COLS) {
        vga[i] = (0x0F << 8) | s[i];
        i++;
    }
    log_row++;
}

static void print_bootinfo(const bootinfo_t *bi) {
    if (!bi) { log_line("No bootinfo struct."); return; }
    if (bi->magic == BOOTINFO_MAGIC_UEFI)
        log_line("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2)
        log_line("[boot] Multiboot2 detected.");
    else
        log_line("[boot] Unknown boot magic!");
    // Print memory map size, cpu count, etc, as needed
}

void kernel_main(bootinfo_t *bootinfo) {
    vga_clear();
    vga_write("Mach Microkernel: Boot OK");
    log_line("");
    print_bootinfo(bootinfo);

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
    log_line("[init] Launch first thread");

    if (!current) { log_line("[PANIC] No thread!"); for(;;) __asm__ volatile("cli; hlt"); }

    __asm__ volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        : : "r"(current->rsp), "r"(current->func) : "memory"
    );

    for (;;) __asm__ volatile("cli; hlt");
}

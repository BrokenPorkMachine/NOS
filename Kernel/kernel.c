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

// --- VGA ---
#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int log_row = 1; // Start below title line

static void vga_clear() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (0x0F << 8) | ' ';
    log_row = 1;
}

static void vga_write(const char* s) {
    volatile uint16_t* vga = (uint16_t*)VGA_TEXT_BUF;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i];
        ++i;
    }
}

static void vga_scroll() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int row = 1; row < VGA_ROWS - 1; ++row) {
        for (int col = 0; col < VGA_COLS; ++col)
            vga[row * VGA_COLS + col] = vga[(row+1) * VGA_COLS + col];
    }
    // Clear last line
    for (int col = 0; col < VGA_COLS; ++col)
        vga[(VGA_ROWS-1) * VGA_COLS + col] = (0x0F << 8) | ' ';
    log_row = VGA_ROWS - 2;
}

static void log_line(const char *s)
{
    if (log_row >= VGA_ROWS-1) vga_scroll();
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF + log_row * VGA_COLS;
    int i = 0;
    while (s[i]) {
        vga[i] = (0x0F << 8) | s[i];
        ++i;
    }
    ++log_row;
}

// --- PANIC macro ---
#define PANIC(msg) do { \
    log_line("[PANIC] " msg); \
    for(;;) __asm__ volatile("cli; hlt"); \
} while (0)

// Optionally support boot info argument:
void kernel_main(void* bootinfo) {
    vga_clear();
    vga_write("Mach Microkernel: Boot OK");
    log_line("Booting up...");

    // Optionally print boot info if present
    if (bootinfo) log_line("[boot] Boot info struct detected");

    log_line("[init] IDT");        idt_install();
    log_line("[init] PIC");        pic_remap();
    log_line("[init] PIT");        pit_init(100);
    log_line("[init] Keyboard");   keyboard_init();
    log_line("[init] Mouse");      mouse_init();
    log_line("[init] NIC");        e1000_init();
    log_line("[init] Paging");     paging_init();
    log_line("[init] Threads");    threads_init();
    log_line("[init] GDT");        gdt_install();
    log_line("[init] Launching first thread...");

    if (!current) PANIC("No initial thread!");

    // --- Bootstrap into first thread ---
    __asm__ volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        :
        : "r"(current->rsp), "r"(current->func)
        : "memory"
    );

    PANIC("kernel_main should never return!");
}

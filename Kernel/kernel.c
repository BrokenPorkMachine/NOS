// src/kernel.c
#include <stdint.h>
#include "../include/bootinfo.h"
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
static void log_line(const char *s) {
    if (log_row >= VGA_ROWS-1) log_row = 1;
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF + log_row * VGA_COLS;
    int i = 0; while (s[i] && i < VGA_COLS) { vga[i] = (0x0F << 8) | s[i]; i++; }
    log_row++;
}
static void utoa(uint64_t val, char *buf, int base) {
    static const char dig[] = "0123456789ABCDEF";
    char tmp[32]; int i = 0, j = 0;
    if (!val) { buf[0] = '0'; buf[1] = 0; return; }
    while (val) { tmp[i++] = dig[val % base]; val /= base; }
    while (i) buf[j++] = tmp[--i]; buf[j] = 0;
}
static void print_bootinfo(const bootinfo_t *bi) {
    char buf[80];
    if (!bi) { log_line("No bootinfo struct."); return; }
    if (bi->magic == BOOTINFO_MAGIC_UEFI) log_line("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2) log_line("[boot] Multiboot2 detected.");
    else log_line("[boot] Unknown boot magic!");

    // Memory map
    utoa(bi->mmap_entries, buf, 10); log_line("[boot] RAM regions:");
    for (uint32_t i = 0; i < bi->mmap_entries; ++i) {
        log_line("  ---------------------");
        utoa(i, buf, 10); log_line(buf);
        utoa((uint64_t)bi->mmap[i].addr, buf, 16); log_line(buf);
        utoa((uint64_t)bi->mmap[i].len, buf, 16); log_line(buf);
        utoa(bi->mmap[i].type, buf, 10); log_line(buf);
    }
    // Framebuffer
    if (bi->framebuffer) {
        utoa(bi->framebuffer->width, buf, 10); log_line("[boot] FB width:"); log_line(buf);
        utoa(bi->framebuffer->height, buf, 10); log_line("[boot] FB height:"); log_line(buf);
        utoa(bi->framebuffer->address, buf, 16); log_line("[boot] FB addr:"); log_line(buf);
    }
    // CPUs
    utoa(bi->cpu_count, buf, 10); log_line("[boot] CPUs detected:"); log_line(buf);
    // ACPI RSDP
    utoa(bi->acpi_rsdp, buf, 16); log_line("[boot] ACPI RSDP:"); log_line(buf);
}
void kernel_main(bootinfo_t *bootinfo) {
    vga_clear();
    log_line("Mach Microkernel: Boot OK");
    log_line("");
    print_bootinfo(bootinfo);
    for (;;) __asm__ volatile("cli; hlt");
}

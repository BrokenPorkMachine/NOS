#include <stdint.h>
#include "../bootloader/include/bootinfo.h"
#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25
#define COLOR(fg, bg) ((bg << 4) | (fg))

static int log_row = 1;

// --- Simple VGA console ---
static void vga_clear() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (COLOR(0xF, 0x0) << 8) | ' ';
    log_row = 1;
}
static void vga_puts(const char *s, int row, int color) {
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF + row * VGA_COLS;
    int i = 0;
    while (s[i] && i < VGA_COLS) {
        vga[i] = (color << 8) | s[i];
        i++;
    }
}
static void log_line_color(const char *s, int color) {
    if (log_row >= VGA_ROWS-1) log_row = 1;
    vga_puts(s, log_row, color);
    log_row++;
}
#define log_line(s) log_line_color((s), COLOR(0xF, 0x0))
#define log_warn(s) log_line_color((s), COLOR(0xE, 0x0))
#define log_info(s) log_line_color((s), COLOR(0xB, 0x0))
#define log_good(s) log_line_color((s), COLOR(0xA, 0x0))

static void utoa(uint64_t val, char *buf, int base) {
    static const char dig[] = "0123456789ABCDEF";
    char tmp[32]; int i = 0, j = 0;
    if (!val) { buf[0] = '0'; buf[1] = 0; return; }
    while (val) { tmp[i++] = dig[val % base]; val /= base; }
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}
static void ptoa(uint64_t val, char *buf) { // Print as 0xHEX
    buf[0] = '0'; buf[1] = 'x';
    utoa(val, buf+2, 16);
}

// --- Bootinfo Memory Type decode ---
static const char *efi_memtype(uint32_t t) {
    switch (t) {
        case 1: return "LoaderCode";
        case 2: return "LoaderData";
        case 3: return "BS_Code";
        case 4: return "BS_Data";
        case 5: return "RT_Code";
        case 6: return "RT_Data";
        case 7: return "ConvRAM";
        case 9: return "ACPI_Rclm";
        case 10: return "ACPI_NVS";
        default: return "?";
    }
}

// --- Print framebuffer pixels (demo text, color bar) ---
static void fb_print_text(const bootinfo_framebuffer_t *fb, const char *s) {
    if (!fb) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb->address;
    uint32_t y = fb->height/2, x = 4;
    for (int i = 0; s[i] && x < fb->width-4; i++, x++)
        pixels[y * (fb->pitch/4) + x] = 0xFFFFFF00 | (s[i]);
}
static void fb_demo_bar(const bootinfo_framebuffer_t *fb) {
    if (!fb) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb->address;
    for (uint32_t y = 0; y < 24 && y < fb->height; ++y)
        for (uint32_t x = 0; x < fb->width; ++x)
            pixels[y * (fb->pitch/4) + x] = (y << 16) | (0xCC << 8) | 0x22;
}

// --- Bootinfo print ---
static void print_bootinfo(const bootinfo_t *bi) {
    char buf[80];
    if (!bi) { log_warn("No bootinfo struct."); return; }
    if (bi->magic == BOOTINFO_MAGIC_UEFI) log_good("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2) log_good("[boot] Multiboot2 detected.");
    else log_warn("[boot] Unknown boot magic!");

    // Memory map
    log_info("[boot] RAM regions:");
    for (uint32_t i = 0; i < bi->mmap_entries; ++i) {
        const bootinfo_memory_t *m = &bi->mmap[i];
        log_line("-------------------------------");
        utoa(i, buf, 10); log_line_color(buf, COLOR(0xC, 0x0));
        ptoa((uint64_t)m->addr, buf); log_line_color(buf, COLOR(0x7, 0x0));
        ptoa((uint64_t)m->len, buf);  log_line_color(buf, COLOR(0x7, 0x0));
        log_line_color(efi_memtype(m->type), COLOR(0xB, 0x0));
    }
    // Framebuffer
    if (bi->framebuffer) {
        utoa(bi->framebuffer->width, buf, 10); log_line("[boot] FB width:"); log_line(buf);
        utoa(bi->framebuffer->height, buf, 10); log_line("[boot] FB height:"); log_line(buf);
        ptoa(bi->framebuffer->address, buf); log_line("[boot] FB addr:"); log_line(buf);
        fb_demo_bar(bi->framebuffer);
        fb_print_text(bi->framebuffer, "Hello from kernel!");
    }
    // CPUs
    utoa(bi->cpu_count, buf, 10); log_line("[boot] CPUs detected:"); log_line(buf);
    // ACPI RSDP
    ptoa(bi->acpi_rsdp, buf); log_line("[boot] ACPI RSDP:"); log_line(buf);

    // TODO: ACPI/MADT parsing, SMP, etc.
}

void kernel_main(bootinfo_t *bootinfo) {
    vga_clear();
    log_good("Mach Microkernel: Boot OK");
    log_line("");
    print_bootinfo(bootinfo);
    for (;;) __asm__ volatile("cli; hlt");
}

#include <stdint.h>
#include "../bootloader/include/bootinfo.h"
#include "../GDT/gdt.h"
#include "../IDT/idt.h"
#include "../IO/pic.h"
#include "../IO/pit.h"
#include "../IO/keyboard.h"
#include "../Task/thread.h"
#include "../VM/pmm.h"
#include "../VM/paging.h"

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25
#define COLOR(fg, bg) (((bg) << 4) | (fg))

static int log_row = 1;

// --- Improved VGA console with scrolling ---
static void vga_clear(void) {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (COLOR(0xF, 0x0) << 8) | ' ';
    log_row = 1;
}

static void vga_scroll(void) {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    // Copy rows 2..24 up by one (skip title row)
    for (int r = 1; r < VGA_ROWS - 1; ++r)
        for (int c = 0; c < VGA_COLS; ++c)
            vga[r * VGA_COLS + c] = vga[(r + 1) * VGA_COLS + c];
    // Clear last line
    for (int c = 0; c < VGA_COLS; ++c)
        vga[(VGA_ROWS - 1) * VGA_COLS + c] = (COLOR(0xF, 0x0) << 8) | ' ';
}

static void vga_puts(const char *s, int row, int color) {
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF + row * VGA_COLS;
    int i = 0;
    while (s[i] && i < VGA_COLS) {
        vga[i] = (color << 8) | (unsigned char)s[i];
        i++;
    }
}

static void log_line_color(const char *s, int color) {
    if (log_row >= VGA_ROWS - 1) {
        vga_scroll();
        log_row = VGA_ROWS - 2;
    }
    vga_puts(s, log_row, color);
    log_row++;
}

#define log_line(s)   log_line_color((s), COLOR(0xF, 0x0))
#define log_warn(s)   log_line_color((s), COLOR(0xE, 0x0))
#define log_info(s)   log_line_color((s), COLOR(0xB, 0x0))
#define log_good(s)   log_line_color((s), COLOR(0xA, 0x0))
#define log_err(s)    log_line_color((s), COLOR(0xC, 0x0))

static void utoa(uint64_t val, char *buf, int base) {
    static const char dig[] = "0123456789ABCDEF";
    char tmp[32]; int i = 0, j = 0;
    if (!val) { buf[0] = '0'; buf[1] = 0; return; }
    while (val) { tmp[i++] = dig[val % base]; val /= base; }
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}
static void ptoa(uint64_t val, char *buf) {
    buf[0] = '0'; buf[1] = 'x';
    utoa(val, buf + 2, 16);
}

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
    if (!fb || !fb->address || !s) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb->address;
    uint32_t y = fb->height / 2, x = 4;
    for (int i = 0; s[i] && x < fb->width - 4; i++, x++)
        pixels[y * (fb->pitch / 4) + x] = 0xFFFFFF00 | (unsigned char)s[i];
}
static void fb_demo_bar(const bootinfo_framebuffer_t *fb) {
    if (!fb || !fb->address) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb->address;
    for (uint32_t y = 0; y < 24 && y < fb->height; ++y)
        for (uint32_t x = 0; x < fb->width; ++x)
            pixels[y * (fb->pitch / 4) + x] = (y << 16) | (0xCC << 8) | 0x22;
}

static void print_bootinfo(const bootinfo_t *bi) {
    char buf[80];
    if (!bi) { log_warn("No bootinfo struct."); return; }
    ptoa((uint64_t)bi, buf); log_line("[boot] bootinfo ptr:"); log_line(buf);
    ptoa((uint64_t)bi->mmap, buf); log_line("[boot] mmap ptr:"); log_line(buf);
    utoa(bi->mmap_entries, buf, 10); log_line("[boot] mmap entries:"); log_line(buf);
    ptoa((uint64_t)bi->framebuffer, buf); log_line("[boot] framebuffer ptr:"); log_line(buf);
    utoa(sizeof(bootinfo_memory_t), buf, 10); log_line("[boot] bootinfo_memory_t size:"); log_line(buf);
    if (bi->magic == BOOTINFO_MAGIC_UEFI) log_good("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2) log_good("[boot] Multiboot2 detected.");
    else log_warn("[boot] Unknown boot magic!");

    uint32_t count = bi->mmap_entries;
    log_info("[boot] RAM regions:");
    for (uint32_t i = 0; i < count && i < 2; ++i) {
        const bootinfo_memory_t *m = &bi->mmap[i];
        log_line("-------------------------------");
        utoa(i, buf, 10); log_line_color(buf, COLOR(0xC, 0x0));
        ptoa((uint64_t)m->addr, buf); log_line_color(buf, COLOR(0x7, 0x0));
        ptoa((uint64_t)m->len, buf);  log_line_color(buf, COLOR(0x7, 0x0));
        log_line_color(efi_memtype(m->type), COLOR(0xB, 0x0));
    }
    if (count > 4) {
        for (uint32_t i = count - 2; i < count; ++i) {
            const bootinfo_memory_t *m = &bi->mmap[i];
            log_line("-------------------------------");
            utoa(i, buf, 10); log_line_color(buf, COLOR(0xC, 0x0));
            ptoa((uint64_t)m->addr, buf); log_line_color(buf, COLOR(0x7, 0x0));
            ptoa((uint64_t)m->len, buf);  log_line_color(buf, COLOR(0x7, 0x0));
            log_line_color(efi_memtype(m->type), COLOR(0xB, 0x0));
        }
    } else {
        for (uint32_t i = 2; i < count; ++i) {
            const bootinfo_memory_t *m = &bi->mmap[i];
            log_line("-------------------------------");
            utoa(i, buf, 10); log_line_color(buf, COLOR(0xC, 0x0));
            ptoa((uint64_t)m->addr, buf); log_line_color(buf, COLOR(0x7, 0x0));
            ptoa((uint64_t)m->len, buf);  log_line_color(buf, COLOR(0x7, 0x0));
            log_line_color(efi_memtype(m->type), COLOR(0xB, 0x0));
        }
    }

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

    // --- ACPI/MADT parsing, SMP enumeration can be inserted here. ---
    // e.g., parse MADT from ACPI, enumerate CPUs, print APIC IDs, etc.
}

void kernel_main(bootinfo_t *bootinfo) {
    vga_clear();
    log_good("Mach Microkernel: Boot OK");
    log_line("");
    if (!bootinfo || bootinfo->size != sizeof(bootinfo_t)) {
        log_err("bootinfo size mismatch");
        for(;;) __asm__("hlt");
    }
    if (!bootinfo->framebuffer) log_line("No framebuffer!");
    if (!bootinfo->mmap) log_line("No mmap!");
    if (bootinfo && bootinfo->mmap_entries > 128) {
        log_err("BUG: mmap_entries too high, halting.");
        for(;;) __asm__("hlt");
    }
    if (bootinfo && bootinfo->mmap_entries > 64)
        log_warn("Warning: suspiciously large mmap_entries");
    print_bootinfo(bootinfo);
    pmm_init(bootinfo);
    paging_init();
    // Initialize core subsystems and start userland services
    gdt_install();
    idt_install();
    pic_remap();
    pit_init(100);
    keyboard_init();

    threads_init();
    asm volatile("sti");

    schedule();
    for (;;) __asm__ volatile("cli; hlt");
}

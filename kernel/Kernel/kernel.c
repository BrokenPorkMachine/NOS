#include <stdint.h>
#include "../../boot/include/bootinfo.h"
#include "../arch/GDT/gdt.h"
#include "../arch/IDT/idt.h"
#include "../drivers/IO/pic.h"
#include "../drivers/IO/pit.h"
#include "../drivers/IO/keyboard.h"
#include "../drivers/IO/mouse.h"
#include "../drivers/IO/serial.h"
#include "../drivers/Net/netstack.h"
#include "../drivers/IO/video.h"
#include "../Task/thread.h"
#include "../VM/pmm.h"
#include "../VM/paging.h"
#include "../VM/cow.h"
#include "../VM/numa.h"
#include "../arch/CPU/cpu.h"
#include "../arch/ACPI/acpi.h"
#include "../arch/CPU/smp.h"

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
    serial_puts(s);
    serial_puts("\n");
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
        case 0: return "Reserved";
        case 1: return "LoaderCode";
        case 2: return "LoaderData";
        case 3: return "BS_Code";
        case 4: return "BS_Data";
        case 5: return "RT_Code";
        case 6: return "RT_Data";
        case 7: return "ConvRAM";
        case 8: return "Unusable";
        case 9: return "ACPI_Rclm";
        case 10: return "ACPI_NVS";
        case 11: return "MMIO";
        case 12: return "MMIOPort";
        case 13: return "PalCode";
        case 14: return "Persistent";
        default: return "?";
    }
}

// Calculate total usable RAM (ConventionalMemory) from boot memory map
static uint64_t calc_total_ram(const bootinfo_t *bi) {
    if (!bi || !bi->mmap) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < bi->mmap_entries; ++i) {
        const bootinfo_memory_t *m = &bi->mmap[i];
        if (m->type == 7) // EfiConventionalMemory
            total += m->len;
    }
    return total;
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
    ptoa((uint64_t)bi->kernel_entry, buf); log_line("[boot] kernel entry:"); log_line(buf);
    if (bi->magic == BOOTINFO_MAGIC_UEFI) log_good("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2) log_good("[boot] Multiboot2 detected.");
    else log_warn("[boot] Unknown boot magic!");

    if (bi->bootloader_name) { log_line("[boot] bootloader:"); log_line(bi->bootloader_name); }
    if (bi->cmdline) { log_line("[boot] cmdline:"); log_line(bi->cmdline); }

    uint64_t ram_bytes = calc_total_ram(bi);
    utoa(ram_bytes >> 20, buf, 10); log_line("[boot] Total RAM (MiB):"); log_line(buf);

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
    log_info("[boot] CPU table:");
    for (uint32_t i = 0; i < bi->cpu_count && i < BOOTINFO_MAX_CPUS; ++i) {
        const bootinfo_cpu_t *c = &bi->cpus[i];
        log_line("-------------------------------");
        log_line("index:");
        utoa(i, buf, 10);              log_line_color(buf, COLOR(0xC, 0x0));
        log_line("processor id:");
        utoa(c->processor_id, buf, 10); log_line_color(buf, COLOR(0x7, 0x0));
        log_line("apic id:");
        utoa(c->apic_id, buf, 10);      log_line_color(buf, COLOR(0x7, 0x0));
        log_line("flags:");
        utoa(c->flags, buf, 16);        log_line_color(buf, COLOR(0xB, 0x0));
    }
    // ACPI RSDP
    ptoa(bi->acpi_rsdp, buf); log_line("[boot] ACPI RSDP:"); log_line(buf);
}

void kernel_main(bootinfo_t *bootinfo) {
    serial_init();
    video_init(bootinfo ? bootinfo->framebuffer : NULL);
    vga_clear();
    log_good("Mach Microkernel: Boot OK");
    log_line("[Stage 1] Validate bootinfo");
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
    if (bootinfo && bootinfo->mmap_entries >= BOOTINFO_MAX_MMAP)
        log_warn("Warning: suspiciously large mmap_entries");
    if (bootinfo && bootinfo->cpu_count > BOOTINFO_MAX_CPUS) {
        log_err("BUG: cpu_count too high, halting.");
        for(;;) __asm__("hlt");
    }
    acpi_init(bootinfo);
    if (bootinfo && bootinfo->cpu_count == 0) {
        bootinfo->cpu_count = cpu_detect_logical_count();
        bootinfo->cpus[0].processor_id = 0;
        bootinfo->cpus[0].apic_id = 0;
        bootinfo->cpus[0].flags = 1;
    }
    print_bootinfo(bootinfo);
    log_line("[Stage 2] Init memory management");
    pmm_init(bootinfo);
    cow_init(pmm_total_frames());
    numa_init(bootinfo);
    paging_init();

    log_line("[Stage 3] Set up interrupts");
    gdt_install();
    idt_install();
    pic_remap();
    pit_init(100);
    keyboard_init();
    log_good("[kbd] Keyboard initialized");
    mouse_init();
    log_good("[mou] Mouse initialized");

    net_init();
    log_good("[net] Network stack ready");

    smp_bootstrap(bootinfo);
    log_line("[Stage 4] Launch servers");
    threads_init();

    log_line("[Stage 5] Scheduler start");
    asm volatile("sti");      // enable interrupts before scheduling threads
    schedule();               // start first thread now that IRQs are enabled

    for (;;) {
        schedule();
    }
}

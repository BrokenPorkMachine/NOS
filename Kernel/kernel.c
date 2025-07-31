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

// Helper: convert 64-bit to hex string
static void hex(char *out, uint64_t v) {
    static const char *h = "0123456789ABCDEF";
    for (int i = 0; i < 16; ++i)
        out[i] = h[(v >> (60 - i*4)) & 0xF];
    out[16] = 0;
}

// VGA helpers
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

// Print all bootinfo goodies
static void print_bootinfo(const bootinfo_t *bi) {
    char line[80], hexbuf[17], hexbuf2[17];
    if (!bi) { log_line("No bootinfo struct."); return; }
    if (bi->magic == BOOTINFO_MAGIC_UEFI)
        log_line("[boot] UEFI detected.");
    else if (bi->magic == BOOTINFO_MAGIC_MB2)
        log_line("[boot] Multiboot2 detected.");
    else
        log_line("[boot] Unknown boot magic!");

    // Print memory map
    log_line("[mem] Physical memory map:");
    for (uint32_t i = 0; i < bi->mmap_entries; ++i) {
        const bootinfo_memory_t *m = &bi->mmap[i];
        hex(hexbuf, m->addr); hex(hexbuf2, m->addr + m->len);
        int n = 0;
        // Use type numbers for now (implement more names later)
        n += snprintf(line, sizeof(line), "  %s-%s type=%u", hexbuf, hexbuf2, m->type);
        line[n] = 0;
        log_line(line);
    }

    // Framebuffer
    if (bi->framebuffer) {
        snprintf(line, sizeof(line),
            "[fb] addr=%p %ux%u pitch=%u bpp=%u",
            (void*)bi->framebuffer->address,
            bi->framebuffer->width, bi->framebuffer->height,
            bi->framebuffer->pitch, bi->framebuffer->bpp
        );
        log_line(line);

        // Fill framebuffer with blue (if bpp >= 24)
        if (bi->framebuffer->bpp >= 24 && bi->framebuffer->type == 0) {
            uint32_t *fb = (uint32_t*)(uintptr_t)bi->framebuffer->address;
            uint32_t color = 0xFF0000FF; // BGRA = blue
            for (uint32_t y = 0; y < bi->framebuffer->height; ++y) {
                for (uint32_t x = 0; x < bi->framebuffer->width; ++x) {
                    fb[y*(bi->framebuffer->pitch/4)+x] = color;
                }
            }
        }
    } else {
        log_line("[fb] No framebuffer provided.");
    }

    // ACPI table
    if (bi->acpi_rsdp) {
        hex(hexbuf, bi->acpi_rsdp);
        snprintf(line, sizeof(line), "[acpi] RSDP at 0x%s", hexbuf);
        log_line(line);
        // (Add ACPI parsing here)
    } else {
        log_line("[acpi] Not present.");
    }

    // SMP (multicore) info
    if (bi->smp) {
        snprintf(line, sizeof(line), "[smp] CPUs detected: %u", bi->smp->count);
        log_line(line);
        for (uint32_t i = 0; i < bi->smp->count; ++i) {
            snprintf(line, sizeof(line), "  CPU %u: APIC ID %u", i, bi->smp->lapic_ids[i]);
            log_line(line);
        }
    } else {
        log_line("[smp] No SMP info.");
    }
}

void kernel_main(bootinfo_t *bootinfo) {
    vga_clear();
    vga_write("Mach Microkernel: Boot OK");
    log_line("");
    print_bootinfo(bootinfo);

    log_line("[init] IDT");         idt_install();
    log_line("[init] PIC");         pic_remap();
    log_line("[init] PIT");         pit_init(100);
    log_line("[init] Keyboard");    keyboard_init();
    log_line("[init] Mouse");       mouse_init();
    log_line("[init] NIC");         e1000_init();
    log_line("[init] Paging");      paging_init();
    log_line("[init] Threads");     threads_init();
    log_line("[init] GDT");         gdt_install();
    log_line("[init] Launch first thread");

    if (!current) { log_line("[PANIC] No thread!"); for(;;) __asm__ volatile("cli; hlt"); }

    __asm__ volatile(
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        : : "r"(current->rsp), "r"(current->func) : "memory"
    );

    for (;;) __asm__ volatile("cli; hlt");
}

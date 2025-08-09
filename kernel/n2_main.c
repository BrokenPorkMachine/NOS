#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "../boot/include/bootinfo.h"
#include "agent.h"
#include "drivers/IO/serial.h"
#include "drivers/IO/video.h"
#include "drivers/IO/tty.h"
#include "drivers/IO/ps2.h"
#include "drivers/IO/block.h"
#include "drivers/IO/sata.h"
#include "drivers/Net/netstack.h"
#include "drivers/IO/usb.h"
#include "drivers/IO/usbkbd.h"
#include "Task/thread.h"
#include "VM/numa.h"
#include "VM/pmm_buddy.h"
#include "VM/kheap.h"
#include "arch/CPU/lapic.h"

// ... (previous kprint, strcspn_local, syscall infrastructure, sandboxing, module loading helpers, hardware/system query helpers, scheduler_loop, etc unchanged) ...

#ifndef VERBOSE
#define VERBOSE 1
#endif

static void kprint(const char *s) { serial_puts(s); }

#if VERBOSE
#define vprint(s) kprint(s)
#else
#define vprint(s) (void)0
#endif
static void print_acpi_info(const bootinfo_t *b) { (void)b; }
static void print_cpu_topology(const bootinfo_t *b) { (void)b; }
static void print_modules(const bootinfo_t *b) { (void)b; }
static void print_framebuffer(const bootinfo_t *b) { (void)b; }
static void print_mmap(const bootinfo_t *b) { (void)b; }
static void load_module(const void *m) { (void)m; }
static void scheduler_loop(void) { while (1) schedule(); }

void n2_main(bootinfo_t *bootinfo) {
    if (!bootinfo || bootinfo->magic != BOOTINFO_MAGIC_UEFI)
        return;

    threads_early_init();
    serial_init();
    vprint("\r\n[N2] NitrOS agent kernel booting...\r\n");
    vprint("[N2] Booted by: ");
    const char *bl = bootinfo->bootloader_name;
    if (bl && ((uintptr_t)bl < 0x100000000ULL)) {
        vprint(bl);
    } else {
        vprint("unknown");
    }
    vprint("\r\n");

    // Framebuffer, ACPI, CPU, modules, memory map, etc.
    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    // --- Memory subsystem init ---
    numa_init(bootinfo);
    buddy_init(bootinfo);      // replaces old pmm_init
    kheap_init();              // kernel heap backed by buddy allocator

    // --- USB support (early, before TTY) ---
    vprint("[N2] Initializing USB stack...\r\n");
    usb_init();       // Initialize USB controller(s)
    usb_kbd_init();   // Set up USB keyboard detection

    // --- Driver/service agent init ---
    const bootinfo_framebuffer_t *fb = (const bootinfo_framebuffer_t *)&bootinfo->fb;
    video_init(fb);
    tty_init();
    ps2_init();
    block_init();
    sata_init();
    net_init();
    vprint("[N2] Starting Agent Registry\r\n");

    // --- Agent system startup ---
    n2_agent_registry_reset();
    vprint("[N2] Agent Registry Reset\r\n");
    // Launch core service threads (e.g., RegX) early
    threads_init();
    vprint("[N2] Launching core service threads\r\n");

    uint64_t rflags, cr0, cr3, cr4;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    uint32_t lapic_tmr = lapic_timer_current();
    serial_printf("[N2] RFLAGS.IF=%lu CR0=%lx CR3=%lx CR4=%lx LAPIC_TMR=%u\n",
                  (rflags >> 9) & 1, cr0, cr3, cr4, lapic_tmr);
    serial_printf("[N2] runqueue len cpu0=%d\n", thread_runqueue_length(0));

    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        load_module(&bootinfo->modules[i]);

    scheduler_loop();
}

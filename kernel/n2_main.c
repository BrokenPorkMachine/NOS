// kernel/n2_main.c
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
#include "uaccess.h"
#include "symbols.h"

// If you already have a trap.h, prefer including it.
// The extern keeps this file standalone if not.
extern void trap_init(void);

// Optional (debug-only) IDT probe/guard hooks. Safe if absent.
void idt_guard_init_once(void) __attribute__((weak));
static inline void call_idt_guard(void) { if (idt_guard_init_once) idt_guard_init_once(); }

extern int timer_ready;

#ifndef VERBOSE
#define VERBOSE 1
#endif

static void kprint(const char *s) { serial_puts(s); }

#if VERBOSE
#define vprint(s) kprint(s)
#else
#define vprint(s) (void)0
#endif

// Stubs if you don’t want extra boot chatter. Fill as needed.
static void print_acpi_info(const bootinfo_t *b)      { (void)b; }
static void print_cpu_topology(const bootinfo_t *b)    { (void)b; }
static void print_modules(const bootinfo_t *b)         { (void)b; }
static void print_framebuffer(const bootinfo_t *b)     { (void)b; }
static void print_mmap(const bootinfo_t *b)            { (void)b; }
static void load_module(const void *m)                 { (void)m; }
static void scheduler_loop(void)                       { for (;;) schedule(); }

void n2_main(bootinfo_t *bootinfo) {
    if (!bootinfo || bootinfo->magic != BOOTINFO_MAGIC_UEFI) return;

    extern char _start, _end;
    symbols_add("kernel", (uintptr_t)&_start, (uintptr_t)&_end - (uintptr_t)&_start);

    threads_early_init();
    serial_init();

    vprint("\r\n[N2] NitrOS agent kernel booting...\r\n");
    vprint("[N2] Booted by: ");
    const char *bl = bootinfo->bootloader_name;
    if (bl) {
        uintptr_t p = (uintptr_t)bl;
        if (is_user_addr(p)) { CANONICAL_GUARD(p); vprint((const char *)p); }
        else vprint("unknown");
    } else vprint("unknown");
    vprint("\r\n");

    // Keep interrupts off until *our* IDT is installed.
    __asm__ volatile("cli");

    // Memory/allocators online first.
    numa_init(bootinfo);
    buddy_init(bootinfo);
    kheap_init();

    // Install OUR kernel IDT before any user code or agent load.
    trap_init();
    // Optional visibility around the firmware vs. kernel IDT state.
    call_idt_guard();

    // Devices (OK now that we have traps):
    vprint("[N2] Initializing USB stack...\r\n");
    usb_init();
    usb_kbd_init();

    const bootinfo_framebuffer_t *fb = (const bootinfo_framebuffer_t *)&bootinfo->fb;
    video_init(fb);
    tty_init();
    ps2_init();
    block_init();
    sata_init();
    net_init();

    vprint("[N2] Starting Agent Registry\r\n");
    n2_agent_registry_reset();
    vprint("[N2] Agent Registry Reset\r\n");

    threads_init();
    vprint("[N2] Launching core service threads\r\n");

    // LAPIC timer for preemption
    lapic_timer_setup_periodic(32, 1000000, 0x3);
    timer_ready = 1;

    // Now that traps are installed, interrupts may be enabled safely.
    __asm__ volatile("sti");

    // Debug registers snapshot
    uint64_t rflags, cr0, cr3, cr4;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    uint32_t lapic_tmr = lapic_timer_current();
    serial_printf("[N2] RFLAGS.IF=%lu CR0=%lx CR3=%lx CR4=%lx LAPIC_TMR=%u\n",
                  (rflags >> 9) & 1, cr0, cr3, cr4, lapic_tmr);
    serial_printf("[N2] runqueue len cpu0=%d\n", thread_runqueue_length(0));

    // Optional boot info spew
    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    // Load any UEFI-provided modules (if you use them)
    for (uint32_t i = 0; i < bootinfo->module_count; ++i) load_module(&bootinfo->modules[i]);

    // Hand scheduler the CPU forever
    scheduler_loop();
}

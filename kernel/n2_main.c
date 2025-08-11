// kernel/n2_main.c  — production-safe bring-up with early IDT sanitization

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

// NEW: sanitize firmware/legacy IDT entries before any agents/threads run
#include "arch/idt_guard.h"

extern int timer_ready;

#ifndef VERBOSE
#define VERBOSE 1
#endif

static void kprint(const char *s) { serial_puts(s); }
#if VERBOSE
#  define vprint(s) kprint(s)
#else
#  define vprint(s) (void)0
#endif

static void print_acpi_info(const bootinfo_t *b) { (void)b; }
static void print_cpu_topology(const bootinfo_t *b) { (void)b; }
static void print_modules(const bootinfo_t *b) { (void)b; }
static void print_framebuffer(const bootinfo_t *b) { (void)b; }
static void print_mmap(const bootinfo_t *b) { (void)b; }
static void load_module(const void *m) { (void)m; }
static void scheduler_loop(void) { while (1) schedule(); }

/* --- Minimal IDT debug helpers (local, no external deps) ------------------ */
struct __attribute__((packed)) idtr64 { uint16_t limit; uint64_t base; };
struct __attribute__((packed)) idt_gate64 {
    uint16_t off0; uint16_t sel; uint8_t ist; uint8_t type_attr;
    uint16_t off1; uint32_t off2; uint32_t zero;
};
static inline void sidt_local(struct idtr64 *d) { __asm__ volatile("sidt %0" : "=m"(*d)); }
static inline uint64_t gate_off(const struct idt_gate64* g) {
    return ((uint64_t)g->off0) | ((uint64_t)g->off1<<16) | ((uint64_t)g->off2<<32);
}
static void idt_debug_probe(const char* tag) {
#if VERBOSE
    struct idtr64 idtr; sidt_local(&idtr);
    const struct idt_gate64* idt = (const struct idt_gate64*)idtr.base;
    uint64_t off6 = gate_off(&idt[6]);
    uint64_t off13 = gate_off(&idt[13]);
    serial_printf("[idt] %s base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  tag, (unsigned long)idtr.base, (unsigned)idtr.limit,
                  (unsigned long)off6, (unsigned long)off13);
#else
    (void)tag;
#endif
}
/* ------------------------------------------------------------------------- */

void n2_main(bootinfo_t *bootinfo)
{
    if (!bootinfo || bootinfo->magic != BOOTINFO_MAGIC_UEFI)
        return;

    // Bring up serial FIRST so we can see early logs
    serial_init();

    vprint("\r\n[N2] NitrOS agent kernel booting...\r\n");
    vprint("[N2] Booted by: ");
    const char *bl = bootinfo->bootloader_name;
    if (bl) {
        uintptr_t p = (uintptr_t)bl;
        if (is_user_addr(p)) {
            CANONICAL_GUARD(p);
            vprint((const char *)p);
        } else {
            vprint("unknown");
        }
    } else {
        vprint("unknown");
    }
    vprint("\r\n");

    // Map kernel symbol range for better crash dumps
    extern char _start, _end;
    symbols_add("kernel", (uintptr_t)&_start, (uintptr_t)&_end - (uintptr_t)&_start);

    // Very early threading primitives (no scheduling yet)
    threads_early_init();

    // Ensure external interrupts are off during IDT work
    __asm__ volatile("cli");

    // BEFORE touching drivers/agents: sanitize the IDT so no vector points into 0xB0000 legacy space.
    idt_debug_probe("pre-guard");
    idt_guard_init_once();
    idt_debug_probe("post-guard");

    // --- Discovery / info (kept quiet by default to avoid early complexity) ---
    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    // --- Memory subsystem init ---
    numa_init(bootinfo);
    buddy_init(bootinfo);   // replaces old pmm_init
    kheap_init();           // kernel heap backed by buddy allocator

    // --- USB first (kbd available for TTY), then display/TTY ---
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

    // Launch scheduler & core services
    threads_init();
    vprint("[N2] Launching core service threads\r\n");

    // Start LAPIC periodic timer (preemptive scheduling if/when IF is enabled)
    lapic_timer_setup_periodic(32, 1000000, 0x3);
    timer_ready = 1;

    // State snapshot for diagnostics
    uint64_t rflags, cr0, cr3, cr4;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    uint32_t lapic_tmr = lapic_timer_current();
    serial_printf("[N2] RFLAGS.IF=%lu CR0=%lx CR3=%lx CR4=%lx LAPIC_TMR=%u\n",
                  (unsigned long)((rflags >> 9) & 1), (unsigned long)cr0,
                  (unsigned long)cr3, (unsigned long)cr4, lapic_tmr);
    serial_printf("[N2] runqueue len cpu0=%d\n", thread_runqueue_length(0));

    // Load boot modules (agents, etc.)
    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        load_module(&bootinfo->modules[i]);

    // NOTE: We deliberately leave IF=0 until your real trap/IRQ setup runs.
    // The guard IDT makes stray vectors harmless in the meantime.
    scheduler_loop();
}

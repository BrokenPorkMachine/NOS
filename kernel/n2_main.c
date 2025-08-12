// kernel/n2_main.c — trimmed to what you posted + safe early guard
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
#include "VM/heap.h"
#include "arch/CPU/lapic.h"
#include "uaccess.h"
#include "symbols.h"
#include "printf.h"

extern int timer_ready;
__attribute__((weak)) void idt_guard_init_once(void);

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

    // Guard: probe/log IDT very early (no SSE, see idt_guard.c)
    if (idt_guard_init_once) idt_guard_init_once();

    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    numa_init(bootinfo);
    buddy_init(bootinfo);
    kheap_parse_bootarg(bootinfo->cmdline);
    kheap_init();

    __asm__ volatile("cli");

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

    lapic_timer_setup_periodic(32, 1000000, 0x3);
    timer_ready = 1;

    uint64_t rflags, cr0, cr3, cr4; uint32_t lapic_tmr;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    lapic_tmr = lapic_timer_current();
    serial_printf("[N2] RFLAGS.IF=%lu CR0=%lx CR3=%lx CR4=%lx LAPIC_TMR=%u\n",
                  (rflags >> 9) & 1, cr0, cr3, cr4, lapic_tmr);
    serial_printf("[N2] runqueue len cpu0=%d\n", thread_runqueue_length(0));

    for (uint32_t i = 0; i < bootinfo->module_count; ++i) load_module(&bootinfo->modules[i]);

    scheduler_loop();
}

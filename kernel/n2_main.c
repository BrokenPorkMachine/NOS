// kernel/n2_main.c — trimmed to what you posted + safe early guard
#include <stdint.h>
#include <stddef.h>
#include "libc.h"
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
#include "arch/GDT/gdt_selectors.h"
#include "arch/IDT/idt.h"
#include "arch_x86_64/gdt_tss.h"
#include "VM/numa.h"
#include "VM/pmm_buddy.h"
#include "VM/heap.h"
#include "arch/APIC/lapic.h"
#include "arch/CPU/irq.h"
#include "uaccess.h"
#include "symbols.h"
#include "printf.h"
#include "nosfs.h"
extern int nosfs_is_ready(void);
extern nosfs_fs_t nosfs_root;
extern void regx_start(void);

extern int timer_ready;
__attribute__((weak)) void idt_guard_init_once(void);

#define LAPIC_TIMER_VECTOR 32

#ifndef VERBOSE
#define VERBOSE 1
#endif

static void kprint(const char *s) { serial_puts(s); }

/* Early bootstrap stack exported from n2_entry.asm */
extern uint8_t _kernel_stack_top[];

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
extern void thread_yield(void);

static void load_module(const void *m)
{
    /* bootinfo_t provides modules with { base, size, name } */
    const struct { void *base; uint64_t size; const char *name; } *mod = m;
    if (!mod || !mod->base || !mod->name)
        return;

    /* Ensure the NOSFS server is ready before populating the filesystem */
    while (!nosfs_is_ready())
        thread_yield();

    const char *name = mod->name;
    if (name[0] == '/')
        name++;

    int h = nosfs_create(&nosfs_root, name, (uint32_t)mod->size, 0);
    if (h >= 0)
        (void)nosfs_write(&nosfs_root, h, 0, mod->base, (uint32_t)mod->size);
}
static void scheduler_loop(void) { while (1) schedule(); }

static void start_timer_interrupts(void) {
    uint64_t f0 = read_rflags();
    kprintf("[init] RFLAGS.IF before: %u\n", (unsigned)((f0 >> 9) & 1));

    lapic_enable();            // enable local APIC (SVR bit 8)
    lapic_timer_init(LAPIC_TIMER_VECTOR); // program LVT timer

    sti();                     // allow interrupts globally

    uint64_t f1 = read_rflags();
    kprintf("[init] RFLAGS.IF after : %u\n", (unsigned)((f1 >> 9) & 1));
}
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

    // Install our full ISR/IRQ table and GDT/TSS before enabling interrupts
    idt_install();
    gdt_tss_init(_kernel_stack_top);
    start_timer_interrupts();

    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    numa_init(bootinfo);
    buddy_init(bootinfo);
    kheap_parse_bootarg(bootinfo->cmdline);
    kheap_init();


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

    timer_ready = 1;

    uint64_t rflags, cr0, cr3, cr4;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    serial_printf("[N2] RFLAGS.IF=%lu CR0=%lx CR3=%lx CR4=%lx\n",
                  (rflags >> 9) & 1, cr0, cr3, cr4);
    serial_printf("[N2] runqueue len cpu0=%d\n", thread_runqueue_length(0));

    for (uint32_t i = 0; i < bootinfo->module_count; ++i) load_module(&bootinfo->modules[i]);
    nosfs_save_device(&nosfs_root, 0);

    regx_start();
    scheduler_loop();
}

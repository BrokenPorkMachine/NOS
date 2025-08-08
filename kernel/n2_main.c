#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../boot/include/bootinfo.h"
#include "agent.h"
#include <nosm.h>
#include "agent_loader.h"
#include <regx.h>
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

extern const uint8_t nosfs_image[] __attribute__((weak));
extern size_t nosfs_size __attribute__((weak));
extern const uint8_t my_mach_agent_image[] __attribute__((weak));
extern size_t my_mach_agent_size __attribute__((weak));

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

    // --- Agent system startup ---
    n2_agent_registry_reset();
    // Launch core service threads (e.g., RegX) early
    threads_init();

    // Load built-in agents if present
    // Validate that the embedded images actually point to a sensible address
    // before handing them to the loader. During development we observed
    // spurious small pointer values (e.g. 0x38) which caused #GP faults when
    // dereferenced by load_agent. Guard against that by insisting the pointer
    // is non‑NULL and outside the low identity‑mapped area.
    if ((uintptr_t)nosfs_image > 0x1000 && nosfs_size > 0 &&
        (uintptr_t)nosfs_image < 0x100000000ULL) {
        load_agent(nosfs_image, nosfs_size, AGENT_FORMAT_NOSM);
    } else {
        vprint("[N2] Builtin NOSFS image missing or invalid\r\n");
    }
    if ((uintptr_t)my_mach_agent_image > 0x1000 && my_mach_agent_size > 0) {
        load_agent(my_mach_agent_image, my_mach_agent_size, AGENT_FORMAT_MACHO2);
    }

    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        load_module(&bootinfo->modules[i]);

    // Query for the default filesystem agent.
    const n2_agent_t *fs = n2_agent_find_capability("filesystem");
    if (fs) {
        vprint("[N2] Filesystem agent active: "); vprint(fs->name); vprint("\r\n");
    } else {
        vprint("[N2] No filesystem agent found!\r\n");
        vprint("make sure regx is starting and that the filesystem agent is started early in the boot\r\n");
        // Halting to avoid crashes when filesystem agent is missing
        for (;;) asm volatile("hlt");
    }

    // Enumerate registered filesystem agents via RegX
    regx_selector_t sel = {0};
    sel.type = REGX_TYPE_FILESYSTEM;
    regx_entry_t agents[4];
    size_t n = regx_enumerate(&sel, agents, 4);
    for (size_t i = 0; i < n; ++i) {
        vprint("[N2] Found filesystem agent: ");
        vprint(agents[i].manifest.name);
        vprint("\r\n");
    }

    scheduler_loop();
}

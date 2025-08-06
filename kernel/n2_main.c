#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../boot/include/bootinfo.h"
#include "agent.h"
#include <nosm.h>
#include "agent_loader.h"
#include "../src/agents/regx/regx.h"
#include "drivers/IO/serial.h"
#include "drivers/IO/video.h"
#include "drivers/IO/tty.h"
#include "drivers/IO/ps2.h"
#include "drivers/IO/block.h"
#include "drivers/IO/sata.h"
#include "drivers/Net/netstack.h"
#include "drivers/IO/usb.h"
#include "drivers/IO/usbkbd.h"

extern const uint8_t nosfs_image[] __attribute__((weak));
extern size_t nosfs_size __attribute__((weak));
extern const uint8_t my_mach_agent_image[] __attribute__((weak));
extern size_t my_mach_agent_size __attribute__((weak));

// ... (previous kprint, strcspn_local, syscall infrastructure, sandboxing, module loading helpers, hardware/system query helpers, scheduler_loop, etc unchanged) ...

static void kprint(const char *s) { serial_puts(s); }
static void print_acpi_info(const bootinfo_t *b) { (void)b; }
static void print_cpu_topology(const bootinfo_t *b) { (void)b; }
static void print_modules(const bootinfo_t *b) { (void)b; }
static void print_framebuffer(const bootinfo_t *b) { (void)b; }
static void print_mmap(const bootinfo_t *b) { (void)b; }
static void load_module(const void *m) { (void)m; }
static void scheduler_loop(void) { for (;;) { } }

void n2_main(bootinfo_t *bootinfo) {
    if (!bootinfo || bootinfo->magic != BOOTINFO_MAGIC_UEFI)
        return;

    serial_init();
    kprint("\r\n[N2] NitrOS agent kernel booting...\r\n");
    kprint("[N2] Booted by: "); kprint(bootinfo->bootloader_name); kprint("\r\n");

    // Framebuffer, ACPI, CPU, modules, memory map, etc.
    print_acpi_info(bootinfo);
    print_cpu_topology(bootinfo);
    print_modules(bootinfo);
    print_framebuffer(bootinfo);
    print_mmap(bootinfo);

    // --- USB support (early, before TTY) ---
    kprint("[N2] Initializing USB stack...\r\n");
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

    // Load built-in agents if present
    if (nosfs_image && nosfs_size)
        load_agent(nosfs_image, nosfs_size, AGENT_FORMAT_NOSM);
    if (my_mach_agent_image && my_mach_agent_size)
        load_agent(my_mach_agent_image, my_mach_agent_size, AGENT_FORMAT_MACHO2);

    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        load_module(&bootinfo->modules[i]);

    // Query for the default filesystem agent.
    const n2_agent_t *fs = n2_agent_find_capability("filesystem");
    if (fs) {
        kprint("[N2] Filesystem agent active: "); kprint(fs->name); kprint("\r\n");
    } else {
        kprint("[N2] No filesystem agent found!\r\n");
    }

    // Enumerate registered filesystem agents via RegX
    regx_selector_t sel = {0};
    sel.type = REGX_TYPE_FILESYSTEM;
    regx_entry_t agents[4];
    size_t n = regx_enumerate(&sel, agents, 4);
    for (size_t i = 0; i < n; ++i) {
        kprint("[N2] Found filesystem agent: ");
        kprint(agents[i].manifest.name);
        kprint("\r\n");
    }

    scheduler_loop();
}

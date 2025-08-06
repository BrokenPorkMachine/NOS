#include "agent_loader.h"
#include "regx.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern const void *nosfs_image;
extern size_t nosfs_size;
extern const void *my_mach_agent_image;
extern size_t my_mach_agent_size;

void n2_main(void) {
    // Load all agent modules at boot
    load_agent(nosfs_image, nosfs_size, AGENT_FORMAT_NOSM);
    load_agent(my_mach_agent_image, my_mach_agent_size, AGENT_FORMAT_MACHO2);

    // Discover any filesystem agent
    regx_selector_t sel = {0};
    sel.type = AGENT_TYPE_FS;
    regx_entry_t agents[4];
    size_t n = regx_enumerate(&sel, agents, 4);
    for (size_t i = 0; i < n; ++i) {
        printf("Found filesystem agent: %s\n", agents[i].manifest.name);
        // Could mount or initialize here...
    }
}

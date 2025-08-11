#include <stdio.h>      // for snprintf
#include "printf.h"     // for serial_printf
#include "agent_loader.h"

// ... inside elf_map_and_spawn(...) after you know `runtime_entry` and `path` ...

// Build a lightweight, always-valid manifest for regx
agent_gate_fn gate = agent_loader_get_gate();
if (gate) {
    // name := basename(path) without extension
    char name[32] = {0};
    if (path && *path) {
        const char *b = path;
        for (const char *p = path; *p; ++p) if (*p=='/'||*p=='\\') b = p+1;
        size_t i = 0;
        while (b[i] && b[i] != '.' && i < sizeof(name)-1) { name[i] = b[i]; ++i; }
        if (i == 0) snprintf(name, sizeof(name), "(elf)");
    } else {
        snprintf(name, sizeof(name), "(elf)");
    }

    const char *entry_sym = "agent_main";   // default for our agents
    char entry_hex[32]; snprintf(entry_hex, sizeof(entry_hex), "%p", (void*)runtime_entry);
    const char *caps = "";                  // no special caps

    serial_printf("[loader] gate: name=\"%s\" entry=\"%s\" @ %s caps=\"%s\" path=\"%s\"\n",
                  name, entry_sym, entry_hex, caps, path ? path : "(null)");
    gate(name, entry_sym, entry_hex, caps, path ? path : "(null)");
}

// Now spawn the mapped image
extern int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);
int rc = -38;
if (__agent_loader_spawn_fn) {
    rc = __agent_loader_spawn_fn(path ? path : "(elf)", (void*)runtime_entry, prio);
    serial_printf("[loader] register_and_spawn rc=%d\n", rc);
}
return rc;

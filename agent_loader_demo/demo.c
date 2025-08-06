#include "agent_loader.h"
#include "regx.h"
#include <stdio.h>
#include <string.h>

/* Dummy entry functions for demonstration */
void elf_agent_main(void) {
    printf("ELF agent executed\n");
}
void macho2_agent_main(void) {
    printf("Mach-O2 agent executed\n");
}

int main(void) {
    /* Register entry points */
    agent_loader_register_entry("elf_agent_main", elf_agent_main);
    agent_loader_register_entry("macho2_agent_main", macho2_agent_main);

    /* Construct a fake Mach-O2 image consisting solely of a manifest JSON. */
    const char macho2_manifest[] =
        "{\"name\":\"macho2_demo\",\"type\":\"demo\",\"version\":\"1.0\"," \
        "\"abi\":\"none\",\"capabilities\":\"example\",\"entry\":\"macho2_agent_main\"}";
    load_agent_auto(macho2_manifest, sizeof(macho2_manifest));

    /* Construct a fake ELF image: ELF magic followed by manifest JSON. */
    const char elf_manifest[] =
        "{\"name\":\"elf_demo\",\"type\":\"demo\",\"version\":\"1.0\"," \
        "\"abi\":\"none\",\"capabilities\":\"example\",\"entry\":\"elf_agent_main\"}";
    unsigned char elf_image[4 + sizeof(elf_manifest)];
    memcpy(elf_image, "\x7F" "ELF", 4);
    memcpy(elf_image + 4, elf_manifest, sizeof(elf_manifest));
    load_agent_auto(elf_image, sizeof(elf_image));

    /* Enumerate registry entries */
    regx_entry_t entries[8];
    size_t count = regx_enumerate(NULL, entries, 8);
    printf("Registered agents:%zu\n", count);
    for (size_t i = 0; i < count; ++i) {
        printf(" - %s (id %llu)\n", entries[i].manifest.name,
               (unsigned long long)entries[i].id);
    }
    return 0;
}

#ifndef AGENT_LOADER_H
#define AGENT_LOADER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Universal agent loader for NitrOS.
 *
 * This loader only performs format detection, minimal manifest extraction
 * and stubbed loading of agents. Real OS integration would replace the
 * mapping and threading stubs with kernel implementations.
 */

typedef enum {
    AGENT_FORMAT_ELF,
    AGENT_FORMAT_MACHO,
    AGENT_FORMAT_MACHO2,
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

/* Map segment stub - real kernel would map memory pages. */
int map_segment(const void *data, size_t size, uint64_t vaddr);
/* Spawn thread stub - real kernel would create new task/thread. */
int spawn_thread(void (*entry)(void));

/* Entry registry used by the demo to resolve manifest symbols. */
typedef void (*agent_entry_t)(void);
int agent_loader_register_entry(const char *name, agent_entry_t fn);
agent_entry_t agent_loader_find_entry(const char *name);

/* Format detection and loaders. */
agent_format_t detect_agent_format(const void *image, size_t size);
int load_agent_auto(const void *image, size_t size);
int load_agent_elf(const void *image, size_t size);
int load_agent_macho(const void *image, size_t size);
int load_agent_macho2(const void *image, size_t size);
int load_agent_flat(const void *image, size_t size);
int load_agent_nosm(const void *image, size_t size);

/* Manifest extraction helpers. */
int extract_manifest_macho2(const void *image, size_t size,
                            char *out_json, size_t out_size);
int extract_manifest_elf(const void *image, size_t size,
                         char *out_json, size_t out_size);

#endif /* AGENT_LOADER_H */

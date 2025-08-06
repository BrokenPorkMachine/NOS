#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../src/agents/regx/regx.h"

// Supported formats
typedef enum {
    AGENT_FORMAT_ELF,
    AGENT_FORMAT_MACHO,
    AGENT_FORMAT_MACHO2,
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

// Detect the format from the buffer
agent_format_t detect_agent_format(const void *image, size_t size);

// Auto-detect and load any supported agent
int load_agent_auto(const void *image, size_t size);

// Explicit format loader. Useful when the caller already knows the
// agent type (e.g., built-in images) and wants to skip detection.
int load_agent(const void *image, size_t size, agent_format_t fmt);

// Format-specific loaders
int load_agent_elf(const void *image, size_t size);
int load_agent_macho(const void *image, size_t size);
int load_agent_macho2(const void *image, size_t size);
int load_agent_flat(const void *image, size_t size);
int load_agent_nosm(const void *image, size_t size);

// Manifest extraction helpers (return 0 on success)
int extract_manifest_macho2(const void *image, size_t size, char *out_json, size_t out_sz);
int extract_manifest_elf(const void *image, size_t size, char *out_json, size_t out_sz);

// Register an entry point by name (used by agents to export their “main”)
typedef void (*agent_entry_t)(void);
void agent_loader_register_entry(const char *name, agent_entry_t fn);

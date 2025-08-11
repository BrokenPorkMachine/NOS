#pragma once
#ifndef AGENT_LOADER_H
#define AGENT_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <regx.h>     // regx_manifest_t, regx_register(), n2_agent_t

/*
 * Supported on-disk/in-memory formats
 */
typedef enum {
    AGENT_FORMAT_ELF = 0,
    AGENT_FORMAT_MACHO,
    AGENT_FORMAT_MACHO2,  // O2-style with embedded JSON manifest
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

/*
 * Entry point type exported by agents (e.g., "init_main")
 */
typedef void (*agent_entry_t)(void);

/*
 * Detect agent format from raw bytes.
 */
agent_format_t detect_agent_format(const void *image, size_t size);

/*
 * Loaders (return 0 on success, <0 on error)
 *  - Auto: detect and load (default prio = 200)
 *  - Auto with explicit priority
 *  - Explicit: caller provides the format (default prio = 200)
 *  - Explicit with priority
 */
int load_agent_auto(const void *image, size_t size);
int load_agent_auto_with_prio(const void *image, size_t size, int prio);

int load_agent(const void *image, size_t size, agent_format_t fmt);
int load_agent_with_prio(const void *image, size_t size, agent_format_t fmt, int prio);

/*
 * NOSM: opaque blob handed to nosm_request_verify_and_load
 * Returns 0 on success, <0 on error.
 */
int load_agent_nosm(const void *image, size_t size);

/*
 * Manifest extraction helpers (copy a JSON object into out_json; 0 on success)
 */
int extract_manifest_macho2(const void *image, size_t size,
                            char *out_json, size_t out_sz);
int extract_manifest_elf(const void *image, size_t size,
                         char *out_json, size_t out_sz);

// Add near the other declarations
int agent_loader_run_from_path(const char *path, int prio);
/*
 * Register an entry point by symbolic name so the loader can resolve "entry"
 * from a manifest to a real function pointer.
 */
void agent_loader_register_entry(const char *name, agent_entry_t fn);

/* ------------------------------------------------------------------------- */
/*                   Kernel integration / extensibility                      */
/* ------------------------------------------------------------------------- */

/* Reader/free hooks so the loader can fetch agent bytes from your FS layer. */
typedef int  (*agent_read_file_fn)(const char *path, void **out, size_t *out_sz);
typedef void (*agent_free_fn)(void *ptr);

void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer);

/*
 * Optional security gate installed by regx: decides whether a load is allowed.
 * Return non-zero to allow, 0 to deny.
 */
typedef int (*agent_gate_fn)(const char *path,
                             const char *name,
                             const char *version,
                             const char *capabilities,
                             const char *entry);

void agent_loader_set_gate(agent_gate_fn gate);

/*
 * Convenience helper: read an agent from the filesystem and load it
 * (auto-detecting format). 'prio' is the initial thread priority.
 */
int agent_loader_run_from_path(const char *path, int prio);

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* AGENT_LOADER_H */

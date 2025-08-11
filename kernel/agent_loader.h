// kernel/agent_loader.h
#pragma once
#include <stddef.h>
#include <stdint.h>

// ----- Agent formats -----
typedef enum {
    AGENT_FORMAT_ELF = 1,
} agent_format_t;

// ----- Security gate callback -----
// Return 0 to allow, nonzero to deny (RegX can decide).
typedef int (*agent_gate_fn)(
    const char *name,        // e.g., "init"
    const char *entry_sym,   // e.g., "agent_main"
    const char *entry_hex,   // e.g., "0x12abc..."
    const char *caps,        // optional caps string
    const char *path         // original path, can be NULL
);

// ----- File I/O callbacks for reading agent images -----
typedef int  (*agent_read_file_fn)(const char *path, const void **data_out, size_t *size_out);
typedef void (*agent_free_fn)(void *p);

// Set/get the optional security gate (RegX will set this)
void           agent_loader_set_gate(agent_gate_fn gate);
agent_gate_fn  agent_loader_get_gate(void);

// Set how the loader reads files (defaults to nosfs if linked)
void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer);

// Public entry points
int load_agent(const void *image, size_t size, agent_format_t fmt);
int load_agent_with_prio(const void *image, size_t size, agent_format_t fmt, int prio);

// Convenience wrapper used by RegX to load from a filesystem path
int agent_loader_run_from_path(const char *path, int prio);

// Bridge provided by agent_loader_pub.c so the loader can spawn a thread
// name: informational (can be path); entry: runtime entry; prio: thread prio
extern int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);

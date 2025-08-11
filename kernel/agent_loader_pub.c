#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "agent_loader.h"  /* for agent_format_t, load_agent() etc. */
#include "nosfs_pub.h"

/* serial_printf is provided by your printf/serial code */
extern int serial_printf(const char *fmt, ...);

/* ---------- Gate (optional: allows regx to audit launches) ---------- */
typedef int (*regx_gate_fn_t)(int tid, uint64_t pc, uint64_t ts);
static regx_gate_fn_t g_gate = 0;

void agent_loader_set_gate(regx_gate_fn_t fn) { g_gate = fn; }

/* ---------- Read provider (defaults to our built-in nosfs) ---------- */
typedef int (*read_file_fn_t)(const char *path, const void **buf, size_t *sz);
static read_file_fn_t g_read = nosfs_read_file;

void agent_loader_set_read(read_file_fn_t fn) {
    g_read = fn ? fn : nosfs_read_file;
}

/* ---------- Simple format auto-detect ---------- */
static agent_format_t detect_fmt(const void *img, size_t sz)
{
    if (sz >= 4) {
        const unsigned char *p = (const unsigned char*)img;
        if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
            return AGENT_FORMAT_ELF;
    }
    /* Add other signatures here if/when needed (Mach-O2, flat, NOSM) */
    return AGENT_FORMAT_UNKNOWN;
}

/* Exported for regx: tries formats and calls core loader. */
int load_agent_auto(const void *image, size_t size)
{
    agent_format_t f = detect_fmt(image, size);
    if (f == AGENT_FORMAT_UNKNOWN) {
        /* Try a couple of likely fallbacks */
        int rc = load_agent(image, size, AGENT_FORMAT_ELF);
        if (rc == 0) return 0;
        /* Add more trials if you add more formats */
        return rc ? rc : -1;
    }
    return load_agent(image, size, f);
}

/* Convenience path-based loader used by thread/regx */
int agent_loader_run_from_path(const char *path)
{
    const void *buf = NULL;
    size_t sz = 0;

    if (!g_read) g_read = nosfs_read_file;

    int rc = g_read(path, &buf, &sz);
    if (rc != 0) {
        serial_printf("[loader] run_from_path \"%s\" read failed rc=%d\n",
                      path ? path : "(null)", rc);
        return rc;
    }
    serial_printf("[loader] run_from_path \"%s\" size=%zu\n",
                  path ? path : "(null)", (unsigned long)sz);

    rc = load_agent_auto(buf, sz);

    /* Inform the gate (optional/soft) */
    if (g_gate) {
        /* We don’t have pc/ts here; pass 0 to indicate “not supplied”. */
        (void)g_gate(/*tid*/0, /*pc*/0, /*ts*/0);
    }
    return rc;
}

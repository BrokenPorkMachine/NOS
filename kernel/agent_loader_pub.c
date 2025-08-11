// kernel/agent_loader_pub.c  -- glue: read-file gate + spawn gate
#include <stddef.h>
#include <stdint.h>

#include "VM/kheap.h"   // kfree
#include "../../include/printf.h"

// Provided by agent_loader.c
int load_agent_auto_path(const char* path, const void* image, size_t size);

// Exported gate symbol used by agent_loader.c
int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio) = 0;

// Optional read function (defaults to nosfs_read_file)
static int (*s_read_file)(const char* path, void** out, size_t* out_sz) = 0;
extern int nosfs_read_file(const char* path, void** out, size_t* out_sz);

// Called by REGX to give us the registry's spawn routine
void agent_loader_set_gate(int (*spawn)(const char*, void*, int))
{
    __agent_loader_spawn_fn = spawn;
}

// Called by threads_init() (or REGX) to provide a file reader
void agent_loader_set_read(int (*rf)(const char*, void**, size_t*))
{
    s_read_file = rf ? rf : nosfs_read_file;
}

// Load+spawn from a NOSFS path
int agent_loader_run_from_path(const char* path)
{
    if (!path) return -22; // EINVAL

    if (!s_read_file) s_read_file = nosfs_read_file;

    void*  buf = 0;
    size_t sz  = 0;
    int rc = s_read_file(path, &buf, &sz);
    if (rc < 0) {
        serial_printf("[loader] run_from_path \"%s\" read rc=%d\n", path, rc);
        return rc;
    }

    serial_printf("[loader] run_from_path \"%s\" size=%zu\n", path, (size_t)sz);
    int lrc = load_agent_auto_path(path, buf, sz);
    if (buf) kfree(buf);
    return lrc;
}

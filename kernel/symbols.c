#include "symbols.h"
#include <string.h>

#define MAX_MODULES 64

typedef struct {
    uintptr_t base;
    size_t    size;
    const char *name;
} module_t;

static module_t modules[MAX_MODULES];
static size_t module_count = 0;

void symbols_add(const char *name, uintptr_t base, size_t size) {
    if (!name || module_count >= MAX_MODULES) return;
    modules[module_count++] = (module_t){ base, size, name };
}

const char *symbols_lookup(uintptr_t addr, uintptr_t *offset) {
    for (size_t i = 0; i < module_count; ++i) {
        uintptr_t start = modules[i].base;
        uintptr_t end   = start + modules[i].size;
        if (addr >= start && addr < end) {
            if (offset) *offset = addr - start;
            return modules[i].name;
        }
    }
    return NULL;
}

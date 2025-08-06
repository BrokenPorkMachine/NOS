#include "regx_ipc.h"

uint64_t regx_ipc_register(const regx_entry_t *entry) {
    return regx_register(entry);
}

int regx_ipc_unregister(uint64_t id) {
    return regx_unregister(id);
}

int regx_ipc_update(uint64_t id, const regx_entry_t *delta) {
    return regx_update(id, delta);
}

const regx_entry_t *regx_ipc_query(uint64_t id) {
    return regx_query(id);
}

size_t regx_ipc_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    return regx_enumerate(sel, out, max);
}

void regx_ipc_tree(uint64_t parent, int level) {
    regx_tree(parent, level);
}

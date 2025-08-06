#pragma once
#include "regx.h"

uint64_t regx_ipc_register(const regx_entry_t *entry);
int      regx_ipc_unregister(uint64_t id);
int      regx_ipc_update(uint64_t id, const regx_entry_t *delta);
const regx_entry_t *regx_ipc_query(uint64_t id);
size_t   regx_ipc_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
void     regx_ipc_tree(uint64_t parent, int level);

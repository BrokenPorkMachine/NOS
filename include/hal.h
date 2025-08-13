#pragma once
#include <stdint.h>
#include <regx.h>

typedef struct {
    regx_type_t type;         // REGX_TYPE_DEVICE, DRIVER, BUS, etc.
    const char *name;         // identifier for the entry
    const char *version;      // optional version string
    const char *abi;          // ABI or interface identifier
    const char *capabilities; // optional capabilities list
} hal_descriptor_t;

void hal_init(void);
void hal_shutdown(void);

uint64_t hal_register(const hal_descriptor_t *desc, uint64_t parent_id);
int      hal_unregister(uint64_t id);
const regx_entry_t *hal_query(uint64_t id);
size_t   hal_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);

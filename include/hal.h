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

// Asynchronous callback signature.
typedef void (*hal_async_cb_t)(uint64_t id, int status, void *ctx);

void hal_init(void);
void hal_shutdown(void);

uint64_t hal_register(const hal_descriptor_t *desc, uint64_t parent_id);
int      hal_unregister(uint64_t id);
const regx_entry_t *hal_query(uint64_t id);
size_t   hal_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);

// Asynchronous variants
void hal_register_async(const hal_descriptor_t *desc, uint64_t parent_id,
                        hal_async_cb_t cb, void *ctx);
void hal_unregister_async(uint64_t id, hal_async_cb_t cb, void *ctx);

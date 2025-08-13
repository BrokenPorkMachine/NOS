#include <string.h>
#include <hal.h>

static uint64_t hal_root_id = 0;

void hal_init(void) {
    if (hal_root_id) return;
    regx_manifest_t m = {0};
    strlcpy(m.name, "hal-root", sizeof(m.name));
    m.type = REGX_TYPE_BUS;
    strlcpy(m.version, "1.0", sizeof(m.version));
    strlcpy(m.abi, "N2-1.0", sizeof(m.abi));
    hal_root_id = regx_register(&m, 0);
}

void hal_shutdown(void) {
    if (hal_root_id) {
        regx_unregister(hal_root_id);
        hal_root_id = 0;
    }
}

static uint64_t default_parent(uint64_t parent_id) {
    if (parent_id) return parent_id;
    if (!hal_root_id) hal_init();
    return hal_root_id;
}

uint64_t hal_register(const hal_descriptor_t *desc, uint64_t parent_id) {
    if (!desc || !desc->name) return 0;

    regx_manifest_t m = {0};
    m.type = desc->type;
    strlcpy(m.name, desc->name, sizeof(m.name));
    if (desc->version)
        strlcpy(m.version, desc->version, sizeof(m.version));
    if (desc->abi)
        strlcpy(m.abi, desc->abi, sizeof(m.abi));
    if (desc->capabilities)
        strlcpy(m.capabilities, desc->capabilities, sizeof(m.capabilities));

    return regx_register(&m, default_parent(parent_id));
}

int hal_unregister(uint64_t id) {
    return regx_unregister(id);
}

const regx_entry_t *hal_query(uint64_t id) {
    return regx_query(id);
}

size_t hal_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max) {
    return regx_enumerate(sel, out, max);
}

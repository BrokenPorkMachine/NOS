#include <assert.h>
#include <stdio.h>
#include <hal.h>

int main(void) {
    hal_init();

    hal_descriptor_t dev = {
        .type = REGX_TYPE_DEVICE,
        .name = "uart0",
        .version = "1.0",
        .abi = "hw",
        .capabilities = "serial"
    };

    uint64_t id = hal_register(&dev, 0);
    assert(id != 0);

    const regx_entry_t *e = hal_query(id);
    assert(e && e->id == id);

    regx_selector_t sel = {
        .type = REGX_TYPE_DEVICE,
        .parent_id = e->parent_id,
    };
    regx_entry_t out[2];
    size_t n = hal_enumerate(&sel, out, 2);
    assert(n == 1);
    assert(out[0].id == id);

    assert(hal_unregister(id) == 0);
    hal_shutdown();
    return 0;
}

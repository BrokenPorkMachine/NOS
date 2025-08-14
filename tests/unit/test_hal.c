#include <assert.h>
#include <stdio.h>
#include <hal.h>

static uint64_t last_id;
static int last_status;

static void reg_cb(uint64_t id, int status, void *ctx) {
    (void)ctx;
    last_id = id;
    last_status = status;
}

static void unreg_cb(uint64_t id, int status, void *ctx) {
    (void)ctx;
    last_id = id;
    last_status = status;
}

int main(void) {
    hal_init();

    hal_descriptor_t dev = {
        .type = REGX_TYPE_DEVICE,
        .name = "uart0",
        .version = "1.0",
        .abi = "hw",
        .capabilities = "serial"
    };

    hal_register_async(&dev, 0, reg_cb, NULL);
    assert(last_status == 0);
    uint64_t id = last_id;
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

    hal_unregister_async(id, unreg_cb, NULL);
    assert(last_status == 0);

    hal_shutdown();
    return 0;
}

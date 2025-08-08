#include "../../include/regx.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    regx_manifest_t m = {0};
    snprintf(m.name, sizeof(m.name), "%s", "fs");
    m.type = REGX_TYPE_FILESYSTEM;
    assert(regx_register(&m, 0) != 0);

    regx_selector_t sel = {0};
    sel.type = REGX_TYPE_FILESYSTEM;

    /* Ensure enumeration with NULL output is safe */
    assert(regx_enumerate(&sel, NULL, 10) == 0);

    /* Normal enumeration returns the entry */
    regx_entry_t out[1];
    size_t n = regx_enumerate(&sel, out, 1);
    assert(n == 1);
    assert(out[0].manifest.type == REGX_TYPE_FILESYSTEM);
    return 0;
}

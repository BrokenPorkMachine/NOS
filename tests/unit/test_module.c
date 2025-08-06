#include <assert.h>
#include "../../kernel/Kernel/module.h"

int main(void) {
    /* Passing NULL should fail */
    assert(kernel_load_module(NULL, 0) == -1);
    return 0;
}

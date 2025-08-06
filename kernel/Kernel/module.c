#include "module.h"
#include "elf.h"
#include <stdint.h>

int kernel_load_module(const void *image, size_t size) {
    (void)size; // size currently unused but kept for future checks
    if (!image)
        return -1;
    void *entry = elf_load(image);
    if (!entry)
        return -1;
    void (*module_entry)(void) = (void (*)(void))entry;
    module_entry();
    return 0;
}

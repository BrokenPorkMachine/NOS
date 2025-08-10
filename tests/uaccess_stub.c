#include <stdint.h>
void __kernel_panic_noncanonical(uint64_t addr, const char* file, int line) {
    (void)addr; (void)file; (void)line;
}

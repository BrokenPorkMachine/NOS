#include <stddef.h>
#include <stdint.h>
void vmm_prot(void* va, size_t size, int prot) {
    (void)va; (void)size; (void)prot;
}
void vmm_switch(uint64_t *new_pml4) { (void)new_pml4; }
static uint64_t dummy_pml4;
uint64_t *vmm_create_pml4(void) { return &dummy_pml4; }

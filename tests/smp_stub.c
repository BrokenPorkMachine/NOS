#include <stdint.h>

static uint32_t current_idx = 0;

void smp_stub_set_cpu_index(uint32_t idx) { current_idx = idx; }

uint32_t smp_cpu_id(void) { return current_idx; }
uint32_t smp_cpu_index(void) { return current_idx; }
uint32_t smp_cpu_count(void) { return 2; }

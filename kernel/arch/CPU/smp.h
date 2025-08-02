#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

void smp_bootstrap(const bootinfo_t *bi);
uint32_t smp_cpu_id(void);
uint32_t smp_cpu_index(void);
uint32_t smp_cpu_count(void);

#pragma once
#include <stdint.h>
#include <stddef.h>

void symbols_add(const char *name, uintptr_t base, size_t size);
const char *symbols_lookup(uintptr_t addr, uintptr_t *offset);

#pragma once
#include <stddef.h>
#include <stdint.h>

int macho2_load(const void *image, size_t size);
void *macho2_find_symbol(const void *image, size_t size, const char *name);


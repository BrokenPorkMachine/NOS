#pragma once
#include <stddef.h>

// Load an ELF kernel module image and execute its entry point.
// Returns 0 on success, -1 on failure.
int kernel_load_module(const void *image, size_t size);

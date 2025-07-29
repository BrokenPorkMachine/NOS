#pragma once
#include <stdint.h>

#define SYSCALL_YIELD 1

void syscall_handle(uint64_t num);

#pragma once
#include <stddef.h>
#include <stdint.h>

void *kalloc(size_t sz);
void  kfree(void *ptr);
void  kheap_init(void);

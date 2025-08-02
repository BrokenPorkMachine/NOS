#pragma once
#include <stdint.h>

void serial_init(void);
void serial_write(char c);
void serial_puts(const char *s);
void serial_puthex(uint32_t value);

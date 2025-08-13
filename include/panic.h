#pragma once
#include <stdarg.h>
void panic(const char *fmt, ...) __attribute__((noreturn));

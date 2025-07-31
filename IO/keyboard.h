#pragma once
#include <stdint.h>

void keyboard_init(void);
int keyboard_read_scancode(void); // returns -1 if none
int keyboard_getchar(void);       // returns ASCII char or -1

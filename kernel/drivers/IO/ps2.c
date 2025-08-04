#include "ps2.h"
#include "keyboard.h"
#include "mouse.h"
#include "serial.h"

void ps2_init(void) {
    serial_puts("PS/2: initializing keyboard and mouse\n");
    keyboard_init();
    mouse_init();
}


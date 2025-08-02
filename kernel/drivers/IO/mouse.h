#pragma once
#include <stdint.h>

struct mouse_packet {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
};

void mouse_init(void);
int mouse_read_packet(struct mouse_packet *p); // returns 0 if packet read

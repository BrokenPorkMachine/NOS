#include <stdint.h>
#include "io.h"
#include "pit.h"

#define PIT_FREQ 1193182
#define PIT_CMD 0x43
#define PIT_CH0 0x40

void pit_init(uint32_t hz) {
    uint16_t divisor = PIT_FREQ / hz;
    outb(PIT_CMD, 0x36); // Channel 0, low/high byte, mode 3, binary
    io_wait();
    outb(PIT_CH0, divisor & 0xFF); // Low byte
    io_wait();
    outb(PIT_CH0, (divisor >> 8) & 0xFF); // High byte
}

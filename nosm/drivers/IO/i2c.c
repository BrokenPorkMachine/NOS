#include "io.h"
#include "i2c.h"
#include "pic.h"
#include "../../../kernel/arch/IDT/context.h"
#include "../../../kernel/arch/APIC/lapic.h"
#include <stddef.h>
#include <stdint.h>

#define I2C_DATA_PORT 0x300
#define I2C_CMD_PORT  0x304
#define I2C_IRQ       10
#define I2C_BUF_SIZE  32

static uint8_t buf[I2C_BUF_SIZE];
static volatile int head, tail;
static void (*data_cb)(uint8_t);

static void enqueue(uint8_t d) {
    int next = (head + 1) % I2C_BUF_SIZE;
    if (next != tail) {
        buf[head] = d;
        head = next;
    }
    if (data_cb) data_cb(d);
}

void i2c_init(void) {
    head = tail = 0;
    data_cb = NULL;
    outb(I2C_CMD_PORT, 0x00); // Reset/enable device
    pic_set_mask(I2C_IRQ, 1); // enable IRQ line
}

void isr_i2c_handler(struct isr_context *ctx) {
    (void)ctx;
    uint8_t d = inb(I2C_DATA_PORT);
    enqueue(d);
    lapic_eoi();
}

int i2c_read(uint8_t *out) {
    if (tail == head) return -1;
    *out = buf[tail];
    tail = (tail + 1) % I2C_BUF_SIZE;
    return 0;
}

void i2c_register_callback(void (*cb)(uint8_t data)) {
    data_cb = cb;
}

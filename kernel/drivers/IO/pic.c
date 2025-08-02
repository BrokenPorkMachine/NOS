#include <stdint.h>
#include "io.h"
#include "pic.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA   (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA   (PIC2+1)

static uint8_t pic1_mask = 0xFF;
static uint8_t pic2_mask = 0xFF;

void pic_remap(void) {
    /* Save current masks - not used since we set new masks explicitly */
    (void)inb(PIC1_DATA);
    (void)inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11); // starts the init
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    outb(PIC1_DATA, 0x20); // remap offset to 32
    io_wait();
    outb(PIC2_DATA, 0x28); // remap offset to 40
    io_wait();
    outb(PIC1_DATA, 0x04); // tell Master PIC there is a slave at IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02); // tell Slave PIC its cascade identity
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Mask all IRQs initially
    pic1_mask = 0xFF;
    pic2_mask = 0xFF;
    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);
}

void pic_set_mask(uint8_t irq, int enable)
{
    if (irq < 8) {
        if (enable) pic1_mask &= ~(1 << irq); else pic1_mask |= (1 << irq);
        outb(PIC1_DATA, pic1_mask);
    } else {
        irq -= 8;
        if (enable) pic2_mask &= ~(1 << irq); else pic2_mask |= (1 << irq);
        outb(PIC2_DATA, pic2_mask);
    }
}

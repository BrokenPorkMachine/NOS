#include <stdint.h>
#include "io.h"
#include "pic.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA   (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA   (PIC2+1)

void pic_remap(void) {
    uint8_t a1, a2;

    a1 = inb(PIC1_DATA); // Save masks
    a2 = inb(PIC2_DATA);

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

    // Unmask timer (IRQ0) and keyboard (IRQ1) while masking others
    // This allows scheduling preemption via PIT and keyboard input
    outb(PIC1_DATA, 0xFC); // 11111100b -> IRQ0 and IRQ1 enabled
    outb(PIC2_DATA, 0xFF); // mask all IRQs on the slave PIC for now
}

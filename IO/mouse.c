#include "io.h"
#include "mouse.h"
#include "pic.h"
#include <stddef.h>

#define MOUSEBUF_SIZE 8
static struct mouse_packet buf[MOUSEBUF_SIZE];
static volatile int head = 0, tail = 0;

static uint8_t packet[3];
static int packet_index = 0;

static void mouse_isr(void);

void mouse_init(void) {
    // Enable auxiliary device
    outb(0x64, 0xA8);
    io_wait();
    // Enable interrupts
    outb(0x64, 0x20);
    io_wait();
    uint8_t status = inb(0x60) | 2;
    outb(0x64, 0x60);
    io_wait();
    outb(0x60, status);
    // Use default settings
    outb(0x64, 0xD4);
    io_wait();
    outb(0x60, 0xF4);
    io_wait();
    inb(0x60); // Ack
    pic_set_mask(12, 1); // enable IRQ12
}

int mouse_read_packet(struct mouse_packet *p) {
    if (tail == head) return -1;
    *p = buf[tail];
    tail = (tail + 1) % MOUSEBUF_SIZE;
    return 0;
}

void mouse_isr(void) {
    uint8_t data = inb(0x60);
    packet[packet_index++] = data;
    if (packet_index == 3) {
        struct mouse_packet mp;
        mp.buttons = packet[0] & 0x07;
        mp.dx = packet[1];
        mp.dy = packet[2];
        int next = (head + 1) % MOUSEBUF_SIZE;
        if (next != tail) {
            buf[head] = mp;
            head = next;
        }
        packet_index = 0;
    }
    outb(0x20, 0x20); // EOI
}

void isr_mouse_handler(void);
void isr_mouse_handler(void) {
    mouse_isr();
}

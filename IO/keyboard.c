#include "io.h"
#include "keyboard.h"
#include <stddef.h>

#define KEYBUF_SIZE 16
static uint8_t keybuf[KEYBUF_SIZE];
static volatile int head = 0, tail = 0;

static void keyboard_isr(void);

void keyboard_init(void) {
    // Nothing special for PS/2 keyboard
    // Handler installed via IDT in idt_install()
}

int keyboard_read_scancode(void) {
    if (tail == head) return -1;
    uint8_t sc = keybuf[tail];
    tail = (tail + 1) % KEYBUF_SIZE;
    return sc;
}

void keyboard_isr(void) {
    uint8_t sc = inb(0x60);
    int next = (head + 1) % KEYBUF_SIZE;
    if (next != tail) {
        keybuf[head] = sc;
        head = next;
    }
    outb(0x20, 0x20); // EOI
}

// Expose handler symbol for assembly stub
void isr_keyboard_handler(void);
void isr_keyboard_handler(void) {
    keyboard_isr();
}

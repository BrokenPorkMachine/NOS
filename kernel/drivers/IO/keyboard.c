#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include <stddef.h>

#define KEYBUF_SIZE 32
static uint8_t keybuf[KEYBUF_SIZE];
static volatile int head = 0, tail = 0;
static int shift_state = 0;

static const char keymap[128] = {
    [1] = 27,
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [12]='-',[13]='=',[14]='\b',[15]='\t',[16]='q',[17]='w',[18]='e',[19]='r',
    [20]='t',[21]='y',[22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',
    [28]='\n',[30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',
    [37]='k',[38]='l',[39]=';',[40]='\'',[41]='`',[44]='z',[45]='x',[46]='c',
    [47]='v',[48]='b',[49]='n',[50]='m',[51]=',',[52]='.',[53]='/',[57]=' '
};

static const char keymap_shift[128] = {
    [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',
    [12]='_',[13]='+',[16]='Q',[17]='W',[18]='E',[19]='R',[20]='T',[21]='Y',[22]='U',
    [23]='I',[24]='O',[25]='P',[26]='{',[27]='}',[30]='A',[31]='S',[32]='D',[33]='F',
    [34]='G',[35]='H',[36]='J',[37]='K',[38]='L',[39]=':',[40]='"',[41]='~',[44]='Z',
    [45]='X',[46]='C',[47]='V',[48]='B',[49]='N',[50]='M',[51]='<',[52]='>',[53]='?',
    [57]=' '
};

static void keyboard_isr(void);

void keyboard_init(void) {
    // Enable the PS/2 keyboard interface and start scanning so keystrokes
    // generate IRQ1 interrupts. Without this, the controller may leave the
    // keyboard disabled and the login server will never receive input.

    // Enable first PS/2 port (keyboard)
    outb(0x64, 0xAE);
    io_wait();

    // Reset to defaults
    outb(0x60, 0xF6); // set defaults
    io_wait();
    (void)inb(0x60); // ack

    // Ensure we use scancode set 1 so the keymap matches the hardware
    outb(0x60, 0xF0); // select scancode set command
    io_wait();
    (void)inb(0x60); // ack
    outb(0x60, 0x01); // use set 1
    io_wait();
    (void)inb(0x60); // ack

    // Enable scanning
    outb(0x60, 0xF4); // enable scanning
    io_wait();
    (void)inb(0x60); // ack

    // Finally unmask keyboard IRQ1 on the PIC
    pic_set_mask(1, 1);

    // Handler is installed via idt_install()
}

int keyboard_read_scancode(void) {
    if (tail == head) return -1;
    uint8_t sc = keybuf[tail];
    tail = (tail + 1) % KEYBUF_SIZE;
    return sc;
}

static char scancode_ascii(uint8_t sc) {
    if (shift_state)
        return (sc < 128) ? keymap_shift[sc] : 0;
    return (sc < 128) ? keymap[sc] : 0;
}

int keyboard_getchar(void) {
    int sc = keyboard_read_scancode();
    if (sc < 0)
        return -1;
    if (sc == 0x2A || sc == 0x36) {
        shift_state = 1;
        return -1;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shift_state = 0;
        return -1;
    }
    if (sc & 0x80)
        return -1; // ignore releases
    char ch = scancode_ascii(sc);
    if (!ch)
        return -1;
    return (unsigned char)ch;
}

void keyboard_isr(void) {
    uint8_t sc = inb(0x60);
    int next = (head + 1) % KEYBUF_SIZE;
    if (next != tail) {
        keybuf[head] = sc;
        head = next;
    }
}

// Expose handler symbol for assembly stub
void isr_keyboard_handler(void);
void isr_keyboard_handler(void) {
    keyboard_isr();
}

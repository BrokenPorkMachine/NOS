#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include <stddef.h>
#include <stdint.h>
#include "../../../kernel/arch/IDT/context.h"

// ========================
// Config and Static State
// ========================
#define KEYBUF_SIZE 32
static uint8_t keybuf[KEYBUF_SIZE];
static volatile int head = 0, tail = 0;
static int shift_state = 0;
static int caps_lock = 0;

// ========================
// Key Maps (Set 1)
// ========================
static const char keymap[128] = {
    [1] = 27, // ESC
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

// ========================
// Keyboard Internal ISR
// ========================
static void keyboard_isr(void) {
    uint8_t sc = inb(0x60);
    int next = (head + 1) % KEYBUF_SIZE;
    if (next != tail) {
        keybuf[head] = sc;
        head = next;
    }
}

// ========================
// Keyboard Initialization
// ========================
void keyboard_init(void) {
    // Enable the PS/2 keyboard interface (controller port 1)
    outb(0x64, 0xAE); io_wait();

    // Reset keyboard to defaults
    outb(0x60, 0xF6); io_wait(); (void)inb(0x60);

    // Set scan code set 1
    outb(0x60, 0xF0); io_wait(); (void)inb(0x60);
    outb(0x60, 0x01); io_wait(); (void)inb(0x60);

    // Enable scanning
    outb(0x60, 0xF4); io_wait(); (void)inb(0x60);

    // Unmask keyboard IRQ1 on the PIC (mask=0 means enabled)
    pic_set_mask(1, 0);

    // The actual ISR is installed via the IDT by kernel/IDT code
}

// ========================
// Scancode Buffer Handling
// ========================
int keyboard_read_scancode(void) {
    if (tail == head) return -1;
    uint8_t sc = keybuf[tail];
    tail = (tail + 1) % KEYBUF_SIZE;
    return sc;
}

// ========================
// ASCII Conversion Helpers
// ========================
static char scancode_ascii(uint8_t sc) {
    char ch = 0;
    if (shift_state)
        ch = (sc < 128) ? keymap_shift[sc] : 0;
    else
        ch = (sc < 128) ? keymap[sc] : 0;
    if (!ch)
        return 0;
    // Only flip case for alpha, not symbols
    if (caps_lock && ch >= 'a' && ch <= 'z')
        ch = ch - 'a' + 'A';
    else if (caps_lock && ch >= 'A' && ch <= 'Z')
        ch = ch - 'A' + 'a';
    return ch;
}

// ========================
// Public: getchar-like API
// ========================
int keyboard_getchar(void) {
    int sc = keyboard_read_scancode();
    if (sc < 0)
        return -1;

    // Handle Shift
    if (sc == 0x2A || sc == 0x36) { shift_state = 1; return -1; }
    if (sc == 0xAA || sc == 0xB6) { shift_state = 0; return -1; }

    // Handle Caps Lock toggle
    if (sc == 0x3A) { caps_lock ^= 1; return -1; }

    // Ignore key releases
    if (sc & 0x80) return -1;

    char ch = scancode_ascii(sc);
    if (!ch)
        return -1;
    return (unsigned char)ch;
}

// ========================
// ISR Handler for Kernel Integration
// ========================
void isr_keyboard_handler(struct isr_context *ctx) {
    (void)ctx; // Not used, but keeps signature uniform with kernel ISRs
    keyboard_isr();
}

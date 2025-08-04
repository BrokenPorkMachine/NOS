#include "tty.h"
#include "keyboard.h"
#include "serial.h"
#include "video.h"

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;

void tty_init(void) {
    tty_clear();
}

void tty_clear(void) {
    video_clear(0);
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (0x0F << 8) | ' ';
    row = 0;
    col = 0;
}

void tty_putc(char c) {
    serial_write(c);
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
    if (c == '\n') {
        col = 0;
        if (++row >= VGA_ROWS - 1)
            row = VGA_ROWS - 2;
        return;
    }
    vga[row * VGA_COLS + col] = (0x0F << 8) | c;
    if (++col >= VGA_COLS) {
        col = 0;
        if (++row >= VGA_ROWS - 1)
            row = VGA_ROWS - 2;
    }
}

void tty_write(const char *s) {
    while (*s)
        tty_putc(*s++);
}

int tty_getchar(void) {
    return keyboard_getchar();
}


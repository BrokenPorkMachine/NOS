#include "tty.h"
#include "keyboard.h"
#include "serial.h"
#include "video.h"
#include "font8x8_basic.h"

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;
static const bootinfo_framebuffer_t *fb_info = NULL;
static uint32_t fb_cols = 0, fb_rows = 0;

static void draw_char_fb(char c, int r, int cpos) {
    if (!fb_info) return;
    const uint8_t *glyph = font8x8_basic[(uint8_t)c];
    for (int y = 0; y < 8; ++y) {
        uint8_t line = glyph[y];
        for (int x = 0; x < 8; ++x) {
            uint32_t color = (line & (1u << x)) ? 0xFFFFFF : 0x000000;
            video_draw_pixel(cpos * 8 + x, r * 8 + y, color);
        }
    }
}

void tty_init(void) {
    fb_info = video_get_info();
    if (fb_info) {
        fb_cols = fb_info->width / 8;
        fb_rows = fb_info->height / 8;
    }
    tty_clear();
}

void tty_clear(void) {
    if (fb_info) {
        video_clear(0);
    } else {
        volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
        for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
            vga[i] = (0x0F << 8) | ' ';
    }
    row = 0;
    col = 0;
}

void tty_putc(char c) {
    serial_write(c);
    if (fb_info) {
        if (c == '\n') {
            col = 0;
            if (++row >= (int)fb_rows)
                row = 0;
            return;
        }
        if (c == '\b') {
            if (col > 0) {
                --col;
                draw_char_fb(' ', row, col);
            }
            return;
        }
        draw_char_fb(c, row, col);
        if (++col >= (int)fb_cols) {
            col = 0;
            if (++row >= (int)fb_rows)
                row = 0;
        }
    } else {
        volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
        if (c == '\n') {
            col = 0;
            if (++row >= VGA_ROWS - 1)
                row = VGA_ROWS - 2;
            return;
        }
        if (c == '\b') {
            if (col > 0) {
                --col;
                vga[row * VGA_COLS + col] = (0x0F << 8) | ' ';
            }
            return;
        }
        vga[row * VGA_COLS + col] = (0x0F << 8) | c;
        if (++col >= VGA_COLS) {
            col = 0;
            if (++row >= VGA_ROWS - 1)
                row = VGA_ROWS - 2;
        }
    }
}

void tty_write(const char *s) {
    while (*s)
        tty_putc(*s++);
}

int tty_getchar(void) {
    return keyboard_getchar();
}


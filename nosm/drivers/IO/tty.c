#include "tty.h"
#include "keyboard.h"
#include "serial.h"
#include "video.h"
#include "font8x8_basic.h"
#include <stddef.h>

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;
static const bootinfo_framebuffer_t *fb_info = NULL;
static uint32_t fb_cols = 0, fb_rows = 0;
static int use_fb = 0;
static int use_vga = 1;

static void draw_char_fb(char c, int r, int cpos) {
    if (!fb_info) return;
    const uint8_t *glyph = font8x8_basic[(uint8_t)c];
    for (int y = 0; y < 8; ++y) {
        uint8_t line = glyph[y];
        for (int x = 0; x < 8; ++x) {
            uint32_t color = (line & (1u << x)) ? 0xFFFFFF : 0x000000;
            int px = cpos * 8 + x;
            int py = r * 16 + y * 2;
            video_draw_pixel(px, py, color);
            video_draw_pixel(px, py + 1, color);
        }
    }
}

void tty_use_vga(int enable) {
    use_vga = enable;
}

void tty_init(void) {
    /* Prefer a linear framebuffer if one is available so unit tests and
     * early boot environments without legacy VGA memory can still emit
     * output safely.  Fallback to the traditional VGA text buffer only when
     * no framebuffer information is present. */
    fb_info = video_get_info();
    if (fb_info) {
        fb_cols = fb_info->width / 8;
        fb_rows = fb_info->height / 16;
        use_fb = 1;
    } else {
        use_fb = 0;
        fb_cols = 0;
        fb_rows = 0;
    }
    tty_clear();
}

void tty_enable_framebuffer(int enable) {
    if (enable) {
        const bootinfo_framebuffer_t *info = video_get_info();
        if (info) {
            fb_info = info;
            fb_cols = fb_info->width / 8;
            fb_rows = fb_info->height / 16;
            use_fb = 1;
            tty_clear();
        }
    } else {
        use_fb = 0;
        fb_info = NULL;
        fb_cols = fb_rows = 0;
        tty_clear();
    }
}

void tty_clear(void) {
    if (use_fb && fb_info) {
        video_clear(0);
    } else if (use_vga) {
        volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
        for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
            vga[i] = (0x0F << 8) | ' ';
    } else {
        /* VGA disabled: nothing to clear */
    }
    row = 0;
    col = 0;
}

void tty_putc(char c) {
    serial_write(c);
    if (use_fb && fb_info) {
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
    } else if (use_vga) {
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
    } else {
        /* VGA disabled: output only to serial */
        if (c == '\n') {
            col = 0;
            if (++row >= VGA_ROWS - 1)
                row = VGA_ROWS - 2;
        } else if (c == '\b') {
            if (col > 0) {
                --col;
            }
        } else {
            if (++col >= VGA_COLS) {
                col = 0;
                if (++row >= VGA_ROWS - 1)
                    row = VGA_ROWS - 2;
            }
        }
    }
}

void tty_write(const char *s) {
    while (*s)
        tty_putc(*s++);
}

int tty_getchar(void) {
    int ch = keyboard_getchar();
    if (ch >= 0)
        return ch;
    return serial_read();
}


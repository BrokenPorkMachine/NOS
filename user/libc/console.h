// src/console.h
#pragma once
#include <stdint.h>

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

enum log_color {
    LOG_DEFAULT = 0x0F, // White on black
    LOG_INFO    = 0x2F, // Green on black
    LOG_WARN    = 0x6E, // Yellow on black
    LOG_ERROR   = 0x4C, // Red on black
    LOG_OKAY    = 0x2A, // Light green on black
};

static int log_row = 0, log_col = 0;

static void vga_clear() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (LOG_DEFAULT << 8) | ' ';
    log_row = 0;
    log_col = 0;
}

// Proper scrolls *all* lines up by one, last line blank.
static void vga_scroll() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int r = 0; r < VGA_ROWS-1; ++r)
        for (int c = 0; c < VGA_COLS; ++c)
            vga[r*VGA_COLS + c] = vga[(r+1)*VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; ++c)
        vga[(VGA_ROWS-1)*VGA_COLS + c] = (LOG_DEFAULT << 8) | ' ';
    if (log_row > 0) log_row--;
}

static void vga_putc(char ch, int color) {
    if (ch == '\n') {
        log_col = 0;
        if (++log_row >= VGA_ROWS)
            vga_scroll();
        return;
    }
    if (log_col >= VGA_COLS) {
        log_col = 0;
        if (++log_row >= VGA_ROWS)
            vga_scroll();
    }
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    vga[log_row * VGA_COLS + log_col] = (color << 8) | ch;
    if (++log_col >= VGA_COLS) {
        log_col = 0;
        if (++log_row >= VGA_ROWS)
            vga_scroll();
    }
}

static void vga_print(const char *s, int color) {
    for (int i = 0; s[i]; ++i)
        vga_putc(s[i], color);
}

// Logging macros (use as LOG_INFO("msg"), LOG_ERROR("msg"), etc)
#define LOG_INFO(msg)    vga_print("[INFO] " msg "\n", LOG_INFO)
#define LOG_WARN(msg)    vga_print("[WARN] " msg "\n", LOG_WARN)
#define LOG_ERROR(msg)   vga_print("[ERR!] " msg "\n", LOG_ERROR)
#define LOG_OK(msg)      vga_print("[ OK ] " msg "\n", LOG_OKAY)
#define LOG(msg)         vga_print(msg "\n", LOG_DEFAULT)



// src/console.h

#pragma once
#include <stdint.h>
#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

enum log_color {
    LOG_DEFAULT = 0x0F, // White on black
    LOG_INFO    = 0x2F, // Green
    LOG_WARN    = 0x6E, // Yellow
    LOG_ERROR   = 0x4C, // Red
};

static int log_row = 1, log_col = 0;

static void vga_clear() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i) vga[i] = (LOG_DEFAULT << 8) | ' ';
    log_row = 1; log_col = 0;
}

static void vga_scroll() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int r = 1; r < VGA_ROWS-1; ++r)
        for (int c = 0; c < VGA_COLS; ++c)
            vga[r*VGA_COLS + c] = vga[(r+1)*VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; ++c)
        vga[(VGA_ROWS-1)*VGA_COLS + c] = (LOG_DEFAULT << 8) | ' ';
    if (log_row > 1) log_row--;
}

static void vga_print(const char *s, int color) {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF + log_row * VGA_COLS;
    int i = 0;
    while (s[i] && log_col < VGA_COLS) {
        if (s[i] == '\n') { log_row++; log_col = 0; }
        else vga[log_col++] = (color << 8) | s[i];
        i++;
    }
    if (++log_row >= VGA_ROWS-1) { vga_scroll(); log_row = VGA_ROWS-2; }
    log_col = 0;
}

#define LOG_INFO(msg)    vga_print("[INFO] " msg, LOG_INFO)
#define LOG_WARN(msg)    vga_print("[WARN] " msg, LOG_WARN)
#define LOG_ERROR(msg)   vga_print("[ERR!] " msg, LOG_ERROR)
#define LOG_OK(msg)      vga_print("[ OK ] " msg, LOG_INFO)

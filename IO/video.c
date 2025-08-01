#include "video.h"
#include <string.h>

static bootinfo_framebuffer_t fb_info;

void video_init(const bootinfo_framebuffer_t *fb) {
    if (fb) fb_info = *fb; else memset(&fb_info, 0, sizeof(fb_info));
}

const bootinfo_framebuffer_t *video_get_info(void) { return fb_info.address ? &fb_info : NULL; }

void video_clear(uint32_t color) {
    if (!fb_info.address) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb_info.address;
    for (uint32_t y = 0; y < fb_info.height; ++y) {
        for (uint32_t x = 0; x < fb_info.width; ++x)
            pixels[y * (fb_info.pitch / 4) + x] = color;
    }
}

void video_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_info.address) return;
    if (x >= fb_info.width || y >= fb_info.height) return;
    uint32_t *pixels = (uint32_t*)(uintptr_t)fb_info.address;
    pixels[y * (fb_info.pitch / 4) + x] = color;
}

void video_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_info.address) return;
    for (uint32_t yy = y; yy < y + h && yy < fb_info.height; ++yy)
        for (uint32_t xx = x; xx < x + w && xx < fb_info.width; ++xx)
            video_draw_pixel(xx, yy, color);
}

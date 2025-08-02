#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

void video_init(const bootinfo_framebuffer_t *fb);
void video_clear(uint32_t color);
void video_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void video_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
const bootinfo_framebuffer_t *video_get_info(void);

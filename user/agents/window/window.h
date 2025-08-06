#pragma once
#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

enum {
    WINDOW_MSG_CREATE = 1,
    WINDOW_MSG_DRAW_RECT,
    WINDOW_MSG_KEY,
};

int window_create(ipc_queue_t *q, uint32_t server,
                  uint32_t x, uint32_t y, uint32_t w, uint32_t h);
int window_draw_rect(ipc_queue_t *q, uint32_t server, uint32_t window,
                     uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
int window_get_event(ipc_queue_t *q, ipc_message_t *ev);

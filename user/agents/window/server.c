#include "server.h"
#include "window.h"
#include "../../libc/libc.h"
#include "../../../nosm/drivers/IO/video.h"
#include "../../../nosm/drivers/IO/keyboard.h"

#define MAX_WINDOWS 8

typedef struct {
    uint32_t id;
    uint32_t owner;
    uint32_t x, y, w, h;
} win_t;

void window_server(ipc_queue_t *q, uint32_t self_id) {
    win_t wins[MAX_WINDOWS];
    memset(wins, 0, sizeof(wins));
    uint32_t next_id = 1;
    int focused = -1;
    ipc_message_t msg;

    while (1) {
        if (ipc_receive(q, self_id, &msg) == 0) {
            switch (msg.type) {
            case WINDOW_MSG_CREATE: {
                int slot = -1;
                for (int i = 0; i < MAX_WINDOWS; ++i) {
                    if (wins[i].id == 0) { slot = i; break; }
                }
                if (slot >= 0) {
                    uint32_t w = 0, h = 0;
                    if (msg.len >= 8) {
                        uint32_t *p = (uint32_t*)msg.data;
                        w = p[0]; h = p[1];
                    }
                    wins[slot].id = next_id++;
                    wins[slot].owner = msg.sender;
                    wins[slot].x = msg.arg1;
                    wins[slot].y = msg.arg2;
                    wins[slot].w = w;
                    wins[slot].h = h;
                    focused = slot;
                    video_fill_rect(wins[slot].x, wins[slot].y, w, h, 0xFFCCCCCC);
                    ipc_message_t reply = {0};
                    reply.type = WINDOW_MSG_CREATE;
                    reply.arg1 = wins[slot].id;
                    ipc_send(q, msg.sender, &reply);
                }
                break; }
            case WINDOW_MSG_DRAW_RECT: {
                uint32_t id = msg.arg1;
                win_t *win = NULL;
                for (int i = 0; i < MAX_WINDOWS; ++i)
                    if (wins[i].id == id) { win = &wins[i]; break; }
                if (win && msg.len >= 20) {
                    uint32_t *p = (uint32_t*)msg.data;
                    uint32_t x = p[0], y = p[1], w = p[2], h = p[3], color = p[4];
                    video_fill_rect(win->x + x, win->y + y, w, h, color);
                }
                break; }
            default:
                break;
            }
        }

        int ch = keyboard_getchar();
        if (ch >= 0 && focused >= 0) {
            ipc_message_t ev = {0};
            ev.type = WINDOW_MSG_KEY;
            ev.arg1 = wins[focused].id;
            ev.arg2 = (uint32_t)ch;
            ipc_send(q, wins[focused].owner, &ev);
        }
    }
}

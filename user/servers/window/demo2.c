#include "demo2.h"
#include "window.h"
#include "../../libc/libc.h"
#include "../../../kernel/drivers/IO/serial.h"

void window_demo2(ipc_queue_t *q, uint32_t self_id, uint32_t server) {
    int win = window_create(q, server, 160, 20, 120, 80);
    if (win < 0)
        return;
    window_draw_rect(q, server, (uint32_t)win, 0, 0, 120, 80, 0xFF0000FF);
    ipc_message_t ev;
    while (window_get_event(q, &ev) == 0) {
        if (ev.type == WINDOW_MSG_KEY && ev.arg1 == (uint32_t)win) {
            char buf[2] = {(char)ev.arg2, '\0'};
            serial_puts(buf);
        }
    }
}

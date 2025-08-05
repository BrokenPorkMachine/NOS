#include <assert.h>
#include <string.h>
#include "../../user/servers/login/login.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"
#include "../../kernel/drivers/IO/tty.h"
#include "bootinfo.h"

static const char *input = "admin\nadmin\n";
static size_t pos = 0;
static int first_poll = 1;

ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;

/* Stubs for serial I/O used by the login server */
void serial_write(char c) { (void)c; }
int serial_read(void) {
    if (first_poll) {
        first_poll = 0;
        return -1; /* simulate initial lack of input */
    }
    if (pos >= strlen(input)) return -1;
    return (unsigned char)input[pos++];
}
void serial_puts(const char *s) { (void)s; }

int keyboard_getchar(void) { return -1; }

/* Minimal framebuffer info so tty uses video path without touching VGA memory */
static bootinfo_framebuffer_t fb = {
    .address = 0,
    .width = 640,
    .height = 480,
    .pitch = 640 * 4,
    .bpp = 32,
    .type = 0,
    .reserved = 0
};

const bootinfo_framebuffer_t *video_get_info(void) { return &fb; }
void video_draw_pixel(int x, int y, uint32_t color) { (void)x; (void)y; (void)color; }
void video_clear(uint32_t color) { (void)color; }

static int yield_count = 0;
void thread_yield(void) { yield_count++; }

static int shell_started = 0;
void shell_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id) {
    (void)fs_q; (void)pkg_q; (void)upd_q; (void)self_id;
    shell_started = 1;
}

int main(void) {
    tty_init();
    ipc_queue_t q; (void)q;
    login_server(&q, 0);
    assert(current_session.active);
    assert(current_session.uid == 0);
    assert(strcmp((const char*)current_session.username, "admin") == 0);
    assert(shell_started);
    assert(yield_count > 0);
    return 0;
}

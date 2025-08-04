#include <assert.h>
#include <string.h>
#include "../../user/servers/login/login.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"

static const char *input = "admin\nadmin\n";
static size_t pos = 0;

ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;

int tty_getchar(void) {
    if (pos >= strlen(input)) return -1;
    return (unsigned char)input[pos++];
}

void tty_putc(char c) { (void)c; }
void tty_write(const char *s) { (void)s; }
void tty_clear(void) { }
void thread_yield(void) { }

static int shell_started = 0;
void shell_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id) {
    (void)fs_q; (void)pkg_q; (void)upd_q; (void)self_id;
    shell_started = 1;
}

int main(void) {
    ipc_queue_t q; (void)q;
    login_server(&q, 0);
    assert(current_session.active);
    assert(current_session.uid == 0);
    assert(strcmp((const char*)current_session.username, "admin") == 0);
    assert(shell_started);
    return 0;
}

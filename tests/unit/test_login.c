#include <assert.h>
#include <string.h>
#include "../../user/servers/login/login.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"

static const char *input = "admin\nadmin\n";
static size_t pos = 0;
static int first_poll = 1;

ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;
ipc_queue_t fs_queue;

/* Stubs for the TTY driver used by the login server */
int tty_getchar(void) {
    if (first_poll) {
        first_poll = 0;
        return -1; /* simulate initial lack of input */
    }
    if (pos >= strlen(input)) return -1;
    return (unsigned char)input[pos++];
}

void tty_write(const char *s) { (void)s; }
void tty_clear(void) { }
void tty_init(void) { }
static int yield_count = 0;
void thread_yield(void) { yield_count++; }

static int nsh_started = 0;
void nsh_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id) {
    (void)fs_q; (void)pkg_q; (void)upd_q; (void)self_id;
    nsh_started = 1;
}

int main(void) {
    ipc_queue_t q; (void)q;
    login_server(&q, 0);
    assert(current_session.active);
    assert(current_session.uid == 0);
    assert(strcmp((const char*)current_session.username, "admin") == 0);
    assert(nsh_started);
    assert(yield_count > 0);
    return 0;
}

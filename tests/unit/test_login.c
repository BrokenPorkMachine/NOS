#include <assert.h>
#include <string.h>
#include "../../user/agents/login/login.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"
#include "../../user/rt/agent_abi.h"

const AgentAPI *NOS = NULL;
uint32_t NOS_TID = 0;

static const char *input = "admin\nadmin\n";
static size_t pos = 0;
static int first_poll = 1;

ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;
ipc_queue_t fs_queue;

/* Stubs for serial I/O used by the login server */
void serial_write(char c) { (void)c; }
void serial_puts(const char *s) { (void)s; }
void serial_init(void) {}
int serial_read(void) {
    if (first_poll) {
        first_poll = 0;
        return -1; /* simulate initial lack of input */
    }
    if (pos >= strlen(input)) return -1;
    return (unsigned char)input[pos++];
}

/* Stubs for TTY output used by the login server */
void tty_clear(void) {}
void tty_write(const char *s) { (void)s; }

int main(void) {
    ipc_queue_t q; (void)q;
    login_server(&q, 0);
    assert(current_session.active);
    assert(current_session.uid == 0);
    assert(strcmp((const char*)current_session.username, "admin") == 0);
    return 0;
}

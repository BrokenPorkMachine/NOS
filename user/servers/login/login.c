#include "login.h"
#include "../../../kernel/drivers/IO/tty.h"
#include "../../../kernel/Task/thread.h"
#include "../../libc/libc.h"
#include "../shell/shell.h"
#include <stddef.h>

volatile login_session_t current_session = {0};

extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

typedef struct {
    const char *user;
    const char *pass;
    uint32_t uid;
} credential_t;

static const credential_t cred_store[] = {
    {"admin", "admin", 0},
    {"guest", "guest", 1},
};
static const size_t cred_count = sizeof(cred_store)/sizeof(cred_store[0]);

static int authenticate(const char *user, const char *pass, const credential_t **out)
{
    for (size_t i = 0; i < cred_count; ++i) {
        if (!strcmp(user, cred_store[i].user) && !strcmp(pass, cred_store[i].pass)) {
            if (out) *out = &cred_store[i];
            return 0;
        }
    }
    return -1;
}

static void puts_out(const char *s)
{
    tty_write(s);
}

static char getchar_block(void)
{
    int ch = -1;
    do {
        thread_yield();
        ch = tty_getchar();
    } while (ch < 0);
    return (char)ch;
}

static void read_line(char *buf, size_t len, int hide)
{
    size_t pos=0;
    for(;;) {
        char c = getchar_block();
        if(c=='\n' || c=='\r') { puts_out("\n"); break; }
        if((c=='\b' || c==127) && pos>0) {
            puts_out("\b \b");
            pos--; continue;
        }
        if(pos+1 < len && c) {
            buf[pos++] = c;
            if(hide) {
                puts_out("*");
            } else {
                char str[2]={c,0};
                puts_out(str);
            }
        }
    }
    buf[pos]=0;
}

void login_server(ipc_queue_t *q, uint32_t self_id)
{
    (void)q; (void)self_id;
    tty_clear();
    puts_out("[login] login server starting\n");
    /*
     * Yield once after initialization so that pending hardware
     * interrupts—particularly keyboard input—are serviced before
     * we begin waiting for user characters.  Without this initial
     * yield the login thread can monopolize the CPU during startup,
     * preventing the keyboard IRQ handler from running and making
     * the prompt appear unresponsive.
     */
    thread_yield();
    char user[32];
    char pass[32];
    for(;;) {
        const credential_t *cred = NULL;
        puts_out("Username: ");
        read_line(user, sizeof(user), 0);
        puts_out("Password: ");
        read_line(pass, sizeof(pass), 1);
        if(authenticate(user, pass, &cred) == 0) {
            puts_out("Login successful\n");
            current_session.uid = cred->uid;
            strncpy((char*)current_session.username, cred->user, sizeof(current_session.username)-1);
            current_session.session_id++;
            current_session.active = 1;
            break;
        } else {
            puts_out("Login failed\n");
        }
    }
    puts_out("[login] starting shell\n");
    shell_main(&fs_queue, &pkg_queue, &upd_queue, self_id);
}

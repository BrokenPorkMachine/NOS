#include "login.h"
#include "../../../kernel/drivers/IO/tty.h"
#include "../../../kernel/Task/thread.h"
#include "../../libc/libc.h"
#include "../shell/shell.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include "../../../kernel/IPC/ipc.h"
#include <stddef.h>
#include <string.h>

volatile login_session_t current_session = {0};

static ipc_queue_t *health_q = NULL;
static uint32_t login_tid = 0;

// Weak fallback so unit tests can link without the full netstack.
__attribute__((weak)) uint32_t net_get_ip(void) { return 0x0A00020F; }

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
static const size_t cred_count = sizeof(cred_store) / sizeof(cred_store[0]);

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
    /*
     * Use the TTY driver so output reaches both the serial console and the
     * framebuffer, making the login prompt visible on local displays.
     */
    tty_write(s);
}

static char getchar_block(void)
{
    int ch;
    /* Poll the TTY until input is available from keyboard or serial. */
    while ((ch = tty_getchar()) < 0) {
        // Respond to health pings while waiting
        if (health_q) {
            ipc_message_t hmsg, hrep = {0};
            if (ipc_receive(health_q, login_tid, &hmsg) == 0 && hmsg.type == IPC_HEALTH_PING) {
                hrep.type = IPC_HEALTH_PONG;
                ipc_send(health_q, login_tid, &hrep);
            }
        }
        thread_yield();
    }
    return (char)ch;
}

static void read_line(char *buf, size_t len, int hide)
{
    size_t pos = 0;
    for (;;) {
        char c = getchar_block();
        if (c == '\n' || c == '\r') {
            puts_out("\n");
            break;
        }
        if ((c == '\b' || c == 127) && pos > 0) {
            puts_out("\b \b");
            --pos;
            continue;
        }
        if (pos + 1 < len) {
            buf[pos++] = c;
            if (hide) {
                puts_out("*");
            } else {
                char str[2] = { c, 0 };
                puts_out(str);
            }
        }
    }
    buf[pos] = '\0';
}

static char *u8_to_str(unsigned v, char *buf)
{
    if (v >= 100) {
        *buf++ = '0' + v / 100;
        v %= 100;
        *buf++ = '0' + v / 10;
        v %= 10;
        *buf++ = '0' + v;
    } else if (v >= 10) {
        *buf++ = '0' + v / 10;
        v %= 10;
        *buf++ = '0' + v;
    } else {
        *buf++ = '0' + v;
    }
    return buf;
}

static void ip_to_str(uint32_t ip, char *buf)
{
    for (int i = 3; i >= 0; --i) {
        unsigned octet = (ip >> (i * 8)) & 0xFF;
        buf = u8_to_str(octet, buf);
        if (i)
            *buf++ = '.';
    }
    *buf = '\0';
}

void login_server(ipc_queue_t *q, uint32_t self_id)
{
    health_q = q;
    login_tid = self_id;
    /* Reinitialize the TTY to ensure output devices are ready before clearing */
    tty_init();
    tty_clear();
    puts_out("[login] login server starting\n");
    uint32_t ip = net_get_ip();
    char ipbuf[16];
    ip_to_str(ip, ipbuf);
    puts_out("IP ");
    puts_out(ipbuf);
    puts_out(" (SSH/VNC)\n");
    thread_yield();

    char user[32];
    char pass[32];

    for (;;) {
        const credential_t *cred = NULL;

        puts_out("Username: ");
        read_line(user, sizeof(user), 0);

        puts_out("Password: ");
        read_line(pass, sizeof(pass), 1);

        if (authenticate(user, pass, &cred) == 0) {
            puts_out("Login successful\n");
            current_session.uid = cred->uid;
            strncpy((char*)current_session.username, cred->user, sizeof(current_session.username) - 1);
            ((char*)current_session.username)[sizeof(current_session.username) - 1] = '\0';
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

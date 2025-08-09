#include "login.h"
#include "../../libc/libc.h"

/*
 * We avoid pulling kernel-private headers. Declare just what we need.
 * In kernel-linked builds, these resolve to real implementations.
 * In standalone tests, the weak stubs below will be used.
 */
__attribute__((weak)) void tty_init(void) {}
__attribute__((weak)) void tty_clear(void) {}
__attribute__((weak)) void tty_write(const char *s) { (void)s; }
__attribute__((weak)) int  tty_getchar(void) { return -1; }

__attribute__((weak)) void thread_yield(void) {}

/* Optional serial helpers (used only for diagnostics). */
__attribute__((weak)) void serial_puts(const char *s) { (void)s; }

/* Basic network helper to print IP at login screen. */
__attribute__((weak)) uint32_t net_get_ip(void) { return 0x0A00020F; } /* 10.0.2.15 default qemu user-net */

/* Weak IPC stubs for standalone builds */
__attribute__((weak)) int ipc_receive(ipc_queue_t *q, uint32_t tid, ipc_message_t *m) {
    (void)q; (void)tid; (void)m; return -1;
}
__attribute__((weak)) int ipc_send(ipc_queue_t *q, uint32_t tid, ipc_message_t *m) {
    (void)q; (void)tid; (void)m; return -1;
}

/* NitroShell entry — weak so the agent can run even without NSH linked. */
__attribute__((weak)) void nsh_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q,
                                    ipc_queue_t *upd_q, uint32_t self_id)
{
    (void)fs_q; (void)pkg_q; (void)upd_q; (void)self_id;
    tty_write("[nsh] stub not implemented\n");
    for (;;) thread_yield();
}

/* Health ping/pong message types must match your ipc.h values. */
#ifndef IPC_HEALTH_PING
#define IPC_HEALTH_PING  0xF1
#endif
#ifndef IPC_HEALTH_PONG
#define IPC_HEALTH_PONG  0xF2
#endif

volatile login_session_t current_session = {0};

static ipc_queue_t *health_q = NULL;
static uint32_t     login_tid = 0;

/* Small helpers (no stdio). */
static void puts_out(const char *s) { tty_write(s); }

static char *u8_to_str(unsigned v, char *buf)
{
    if (v >= 100) { *buf++ = '0' + v/100; v %= 100; }
    if (v >= 10)  { *buf++ = '0' + v/10;  v %= 10;  }
    *buf++ = '0' + v;
    return buf;
}

static void ip_to_str(uint32_t ip, char *buf)
{
    for (int i = 3; i >= 0; --i) {
        unsigned o = (ip >> (i*8)) & 0xFF;
        buf = u8_to_str(o, buf);
        if (i) *buf++ = '.';
    }
    *buf = '\0';
}

static char getchar_block(void)
{
    int ch;
    for (;;) {
        ch = tty_getchar();
        if (ch >= 0) return (char)ch;

        /* Answer health pings while idle */
        if (health_q) {
            ipc_message_t m = {0}, r = {0};
            if (ipc_receive(health_q, login_tid, &m) == 0 && m.type == IPC_HEALTH_PING) {
                r.type = IPC_HEALTH_PONG;
                ipc_send(health_q, login_tid, &r);
            }
        }
        thread_yield();
    }
}

static void read_line(char *buf, size_t sz, int echo_asterisk)
{
    size_t n = 0;
    for (;;) {
        char c = getchar_block();
        if (c == '\r' || c == '\n') { puts_out("\n"); break; }
        if ((c == '\b' || c == 127) && n) { puts_out("\b \b"); --n; continue; }
        if (n + 1 < sz) {
            buf[n++] = c;
            if (echo_asterisk) puts_out("*");
            else { char t[2] = {c, 0}; puts_out(t); }
        }
    }
    buf[n] = 0;
}

/*
 * Manifest for Mach-O2 container (your loader is already looking for this).
 * Change "entry" if you rename the function below.
 */
void login_server(ipc_queue_t *fs_q, uint32_t self_id)
{
    (void)fs_q;
    health_q = fs_q;
    login_tid = self_id;

    tty_init();
    tty_clear();

    puts_out("[login] server starting\n");

    /* Show IP to help users know where to SSH/VNC. */
    char ipbuf[16];
    ip_to_str(net_get_ip(), ipbuf);
    puts_out("IP ");
    puts_out(ipbuf);
    puts_out(" (SSH/VNC)\n\n");

    char user[32], pass[32];

    /* trivial built-in auth db */
    typedef struct { const char *u, *p; uint32_t uid; } cred_t;
    static const cred_t creds[] = {
        {"admin","admin",0},
        {"guest","guest",1},
    };

    for (;;) {
        const cred_t *hit = NULL;

        puts_out("Username: ");
        read_line(user, sizeof(user), 0);

        puts_out("Password: ");
        read_line(pass, sizeof(pass), 1);

        for (size_t i = 0; i < sizeof(creds)/sizeof(creds[0]); ++i) {
            if (!strcmp(user, creds[i].u) && !strcmp(pass, creds[i].p)) {
                hit = &creds[i];
                break;
            }
        }

        if (hit) {
            puts_out("Login successful\n\n");
            current_session.uid = hit->uid;
            current_session.session_id++;
            current_session.active = 1;
            /* write username safely */
            strlcpy(current_session.username, hit->u, sizeof(current_session.username));
            break;
        } else {
            puts_out("Login failed\n\n");
        }
    }

    puts_out("[login] launching nsh...\n");
    /* Launch NitroShell; pass NULL queues for now (keep ABI stable). */
    nsh_main(NULL, NULL, NULL, self_id);

    /* nsh_main should not return; if it does, idle safely */
    for (;;) thread_yield();
}

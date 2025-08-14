#include "login.h"

extern int kprintf(const char *fmt, ...);

/* Minimal manifest so the loader can discover the entry point when the
 * agent is packaged as a Mach-O2 binary. */
__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char manifest[] =
"{\n"
"  \"name\": \"login\",\n"
"  \"type\": 4,\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"login_server\"\n"
"}\n";

/* Provide weak stubs for the minimal services the login agent relies on.
 * In standalone unit tests these will be overridden by test-specific
 * implementations. */
__attribute__((weak)) void tty_init(void) {}
__attribute__((weak)) void tty_clear(void) {}
__attribute__((weak)) void tty_write(const char *s) { (void)s; }
__attribute__((weak)) int  tty_getchar(void) { return -1; }
__attribute__((weak)) void tty_enable_framebuffer(int enable) { (void)enable; }
__attribute__((weak)) void thread_yield(void) {}

/* NitroShell entry point.  In tests a custom implementation is linked in. */
__attribute__((weak)) void nsh_main(void *fs_q, void *pkg_q, void *upd_q,
                                    uint32_t self_id) {
    (void)fs_q; (void)pkg_q; (void)upd_q; (void)self_id;
    for (;;) thread_yield();
}

volatile login_session_t current_session = {0};

/* Simple output helper (no stdio). */
static void put_str(const char *s) { tty_write(s); }

/* Block until a character is available from the TTY.  While waiting we
 * yield so the scheduler can run other work. */
static char getchar_block(void) {
    int ch;
    for (;;) {
        ch = tty_getchar();
        if (ch >= 0) return (char)ch;
        thread_yield();
    }
}

/* Read a line of text from the TTY.  The caller decides whether characters
 * are echoed back or replaced with asterisks (for passwords). */
static void read_line(char *buf, size_t sz, int echo_asterisk) {
    size_t n = 0;
    for (;;) {
        char c = getchar_block();
        if (c == '\n' || c == '\r') { put_str("\n"); break; }
        if ((c == '\b' || c == 127) && n) {
            put_str("\b \b");
            --n;
            continue;
        }
        if (n + 1 < sz) {
            buf[n++] = c;
            if (echo_asterisk) {
                put_str("*");
            } else {
                char t[2] = {c, 0};
                put_str(t);
            }
        }
    }
    buf[n] = 0;
}

/* Public entry point for the login server.  fs_q is currently unused but
 * kept for ABI compatibility with the real system. */
void login_server(void *fs_q, uint32_t self_id) {
    (void)fs_q;
    (void)self_id;

    /* Console is expected to be initialized by the system before starting the login server. */
    /* Ensure framebuffer output is used when available to avoid touching VGA memory. */
    tty_enable_framebuffer(1);
    tty_clear();
    kprintf("[login] server starting\n");
    put_str("[login] server starting\n");

    char user[32];
    char pass[32];

    for (;;) {
        put_str("Username: ");
        read_line(user, sizeof(user), 0);

        put_str("Password: ");
        read_line(pass, sizeof(pass), 1);

        if (!strcmp(user, "admin") && !strcmp(pass, "admin")) {
            current_session.uid = 0;
            strlcpy((char*)current_session.username, "admin",
                    sizeof(current_session.username));
        } else if (!strcmp(user, "guest") && !strcmp(pass, "guest")) {
            current_session.uid = 1;
            strlcpy((char*)current_session.username, "guest",
                    sizeof(current_session.username));
        } else {
            put_str("Login failed\n\n");
            continue;
        }

        current_session.session_id++;
        current_session.active = 1;
        put_str("Login successful\n\n");
        break;
    }

    /* Start NitroShell; queues are not yet wired up so NULLs are passed. */
    nsh_main(NULL, NULL, NULL, self_id);

#ifndef LOGIN_UNIT_TEST
    for (;;) thread_yield();
#endif
}


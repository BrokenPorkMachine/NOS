#include "login.h"
#include "../../rt/agent_abi.h"
#ifndef LOGIN_UNIT_TEST
#include "../../../nosm/drivers/IO/serial.c"
#else
#include "../../../nosm/drivers/IO/serial.h"
#endif
#include "../../../nosm/drivers/IO/tty.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static FILE *console = NULL;
volatile login_session_t current_session = {0};

/* Always emit output through the TTY so the login prompt is visible even
 * when /dev/console is a non-graphical handle.  Also mirror to the kernel's
 * puts hook when available for debugging. */
static void put_str(const char *s) {
    tty_write(s);
    if (NOS && NOS->puts)
        NOS->puts(s);
    if (console) {
        fwrite(s, 1, strlen(s), console);
        fflush(console);
    } else {
        /* Prefer the graphical TTY when no /dev/console handle is available. */
        tty_write(s);
        if (NOS && NOS->puts)
            NOS->puts(s);
    }
}

/* Block until a character is available from the chosen input device. */
static char getchar_block(void) {
    if (console) {
        unsigned char c;
        while (fread(&c, 1, 1, console) != 1) {
            for (volatile int i = 0; i < 10000; ++i)
                __asm__ __volatile__("pause");
        }
        return (char)c;
    }

    int ch;
    for (;;) {
        ch = serial_read();
        if (ch >= 0) return (char)ch;
        for (volatile int i = 0; i < 10000; ++i)
            __asm__ __volatile__("pause");
    }
}

/* Read a line of text from the active console. */
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

/* Public entry point for the login server. */
void login_server(void *fs_q, uint32_t self_id) {
    (void)fs_q; (void)self_id;

    serial_init();
#ifndef LOGIN_UNIT_TEST
    /* Open the console only for input and rely on the TTY for output. */
    console = fopen("/dev/console", "r");
#endif
    tty_clear();
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

#ifndef LOGIN_UNIT_TEST
    for (;;) {
        for (volatile int i = 0; i < 200000; ++i)
            __asm__ __volatile__("pause");
    }
#endif
}


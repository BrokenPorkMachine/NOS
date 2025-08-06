#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#define NOSFS_NAME_LEN 32
#include "../../../kernel/drivers/IO/tty.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Skeleton NitroShell implementation.
 * Provides basic prompt rendering, input loop, and
 * stubs for agent discovery and command dispatch.
 * Real agent IPC, completion, and scripting will be
 * layered on top of these placeholders. */

#define MAX_ARGS 8

static void putc_out(char c) { tty_putc(c); }
static void puts_out(const char *s) { tty_write(s); }

/* Basic line editing (backspace support) */
static char getchar_block(void) {
    int ch = -1;
    while (ch < 0) {
        ch = tty_getchar();
    }
    return (char)ch;
}

static void read_line(char *buf, size_t len) {
    size_t pos = 0;
    for (;;) {
        char c = getchar_block();
        if (c == '\n' || c == '\r') {
            putc_out('\n');
            break;
        }
        if ((c == '\b' || c == 127) && pos > 0) {
            putc_out('\b'); putc_out(' '); putc_out('\b');
            pos--; continue;
        }
        if (pos + 1 < len) {
            buf[pos++] = c;
            putc_out(c);
        }
    }
    buf[pos] = '\0';
}

static int tokenize(char *line, char *argv[], int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; p++; }
    }
    return argc;
}

static char cwd[NOSFS_NAME_LEN] = "/";

/* Stub: query registry for agents.
 * Real implementation will use IPC to discover
 * agents providing shell extensions and completions. */
static void discover_agents(ipc_queue_t *registry_q, uint32_t self_id) {
    (void)registry_q; (void)self_id;
    /* TODO: send registry query and populate data structures */
}

static void render_prompt(void) {
    puts_out("nsh:");
    puts_out(cwd);
    puts_out("$ ");
}

static void dispatch_command(int argc, char **argv) {
    if (argc == 0) return;
    /* TODO: route commands to agents or builtins */
    puts_out("command: ");
    puts_out(argv[0]);
    putc_out('\n');
}

void nsh_main(ipc_queue_t *registry_q, uint32_t self_id) {
    discover_agents(registry_q, self_id);
    char line[128];
    char *argv[MAX_ARGS];
    for (;;) {
        render_prompt();
        read_line(line, sizeof(line));
        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc > 0 && strcmp(argv[0], "exit") == 0)
            break;
        dispatch_command(argc, argv);
    }
    puts_out("bye\n");
}

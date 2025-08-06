#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include "../pkg/pkg.h"
#include "../update/update.h"
#include "../../include/nosfs.h"
#include "../nosfs/nosfs_server.h"
#include "../../../kernel/drivers/IO/tty.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Write to TTY
static void putc_out(char c) { tty_putc(c); }
static void puts_out(const char *s) { tty_write(s); }

static char cwd[NOSFS_NAME_LEN] = "/";

static void build_path(const char *name, char out[NOSFS_NAME_LEN]) {
    if (name[0] == '/') {
        strncpy(out, name + 1, NOSFS_NAME_LEN - 1);
        out[NOSFS_NAME_LEN - 1] = '\0';
        return;
    }
    size_t len = strlen(cwd);
    strncpy(out, cwd, NOSFS_NAME_LEN - 1);
    out[NOSFS_NAME_LEN - 1] = '\0';
    if (len && len < NOSFS_NAME_LEN - 1)
        out[len++] = '/';
    strncpy(out + len, name, NOSFS_NAME_LEN - len - 1);
    out[NOSFS_NAME_LEN - 1] = '\0';
}

static inline void sys_yield(void) { asm volatile("mov $0, %%rax; int $0x80" ::: "rax"); }

static char getchar_block(void) {
    int ch = -1;
    while (ch < 0) {
        ch = tty_getchar();
        if (ch < 0)
            sys_yield();
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
            pos--;
            continue;
        }
        if (pos + 1 < len && c) {
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

// --- Filesystem IPC helpers and commands ---
// [Insert your cmd_ls, cmd_cat, cmd_create, cmd_write, cmd_rm, cmd_mv, cmd_crc, cmd_verify, cmd_cd, cmd_mkdir,
//  cmd_install_pkg, cmd_uninstall_pkg, cmd_pkg_list, cmd_update, cmd_help functions here, unchanged from codex branch]

// Example: (shortened for brevity)
static void cmd_help(void) {
    puts_out("Available commands:\n");
    puts_out("  ls        - list files\n");
    puts_out("  cat FILE  - display file contents\n");
    puts_out("  create FILE - create a new file\n");
    puts_out("  write FILE DATA - write data to file\n");
    puts_out("  rm FILE   - delete a file\n");
    puts_out("  mv OLD NEW - rename file\n");
    puts_out("  crc FILE  - compute CRC32\n");
    puts_out("  verify FILE - verify CRC32\n");
    puts_out("  install PKG - install package\n");
    puts_out("  uninstall PKG - remove package\n");
    puts_out("  pkglist    - list installed packages\n");
    puts_out("  update TARGET - update kernel or userland\n");
    puts_out("  cd DIR    - change directory\n");
    puts_out("  mkdir DIR - make directory\n");
    puts_out("  help      - show this message\n");
}

// --- Modern prompt rendering ---
static void render_prompt(void) {
    puts_out("nsh:");
    puts_out(cwd);
    puts_out("$ ");
}

// --- NitroShell main loop ---
void nsh_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id) {
    tty_clear();
    puts_out("NitroShell ready\n");
    puts_out("type 'help' for commands\n");
    char line[128], *argv[8];
    for (;;) {
        render_prompt();
        read_line(line, sizeof(line));
        int argc = tokenize(line, argv, 8);
        if (argc == 0) continue;

        if (!strcmp(argv[0], "ls") || !strcmp(argv[0], "dir")) {
            cmd_ls(fs_q, self_id);
        } else if (!strcmp(argv[0], "cat") && argc > 1) {
            cmd_cat(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "create") && argc > 1) {
            cmd_create(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "write") && argc > 2) {
            cmd_write(fs_q, self_id, argv[1], argv[2]);
        } else if (!strcmp(argv[0], "rm") && argc > 1) {
            cmd_rm(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "mv") && argc > 2) {
            cmd_mv(fs_q, self_id, argv[1], argv[2]);
        } else if (!strcmp(argv[0], "crc") && argc > 1) {
            cmd_crc(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "verify") && argc > 1) {
            cmd_verify(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "cd") && argc > 1) {
            cmd_cd(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "mkdir") && argc > 1) {
            cmd_mkdir(fs_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "install") && argc > 1) {
            cmd_install_pkg(pkg_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "uninstall") && argc > 1) {
            cmd_uninstall_pkg(pkg_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "pkglist")) {
            cmd_pkg_list(pkg_q, self_id);
        } else if (!strcmp(argv[0], "update") && argc > 1) {
            cmd_update(upd_q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "help")) {
            cmd_help();
        } else if (!strcmp(argv[0], "exit")) {
            break;
        } else {
            puts_out("?\n");
        }
    }
    puts_out("bye\n");
}

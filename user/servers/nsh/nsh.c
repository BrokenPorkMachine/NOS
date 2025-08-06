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

// Optional: #include "console.h" for log colors (see LOG_* macros at bottom)

// Write to TTY
static void putc_out(char c) { tty_putc(c); }
static void puts_out(const char *s) { tty_write(s); }

static char cwd[NOSFS_NAME_LEN] = "";

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

static void u32_hex(uint32_t v, char buf[9]) {
    for (int i = 7; i >= 0; --i) {
        buf[i] = "0123456789ABCDEF"[v & 0xF];
        v >>= 4;
    }
    buf[8] = '\0';
}

// --- Input (line editing, supports backspace) ---
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
        if (c == '\b' || c == 127) { // Backspace
            if (pos > 0) {
                putc_out('\b');
                putc_out(' ');
                putc_out('\b');
                pos--;
            }
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

// --- Filesystem IPC helpers ---
static int find_handle(ipc_queue_t *q, uint32_t self_id, const char *name) {
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_LIST;
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        char *n = (char *)reply.data + i * NOSFS_NAME_LEN;
        if (strncmp(n, path, NOSFS_NAME_LEN) == 0)
            return i;
    }
    return -1;
}

// --- Commands ---
static void cmd_ls(ipc_queue_t *q, uint32_t self_id) {
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_LIST; msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        const char *n = (char *)reply.data + i * NOSFS_NAME_LEN;
        size_t pref = strlen(cwd);
        if (pref && strncmp(n, cwd, pref) != 0)
            continue;
        if (pref && n[pref] == '/')
            n += pref + 1;
        puts_out(n);
        putc_out('\n');
    }
}
static void cmd_cat(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_READ; msg.arg1 = h; msg.arg2 = 0;
    msg.len = IPC_MSG_DATA_MAX;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    if (reply.arg1 != 0) { puts_out("read error\n"); return; }
    reply.data[reply.len] = '\0';
    puts_out((char *)reply.data);
    putc_out('\n');
}
static void cmd_create(ipc_queue_t *q, uint32_t self_id, const char *name) {
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CREATE; msg.arg1 = IPC_MSG_DATA_MAX;
    msg.arg2 = NOSFS_PERM_READ | NOSFS_PERM_WRITE;
    size_t len = strlen(path);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    strncpy((char *)msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "created\n" : "error\n");
}
static void cmd_write(ipc_queue_t *q, uint32_t self_id, const char *name, const char *data) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    size_t len = strlen(data);
    if (len > IPC_MSG_DATA_MAX) len = IPC_MSG_DATA_MAX;
    msg.type = NOSFS_MSG_WRITE; msg.arg1 = h; msg.arg2 = 0;
    memcpy(msg.data, data, len); msg.len = len;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "ok\n" : "error\n");
}
static void cmd_rm(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_DELETE; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "deleted\n" : "error\n");
}
static void cmd_mv(ipc_queue_t *q, uint32_t self_id, const char *old, const char *new) {
    int h = find_handle(q, self_id, old);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_RENAME;
    msg.arg1 = h;
    char path[NOSFS_NAME_LEN];
    build_path(new, path);
    size_t len = strlen(path);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    strncpy((char *)msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "moved\n" : "error\n");
}

static void cmd_crc(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CRC; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    if (reply.arg1 != 0) { puts_out("error\n"); return; }
    char buf[9];
    u32_hex(reply.arg2, buf);
    puts_out("crc: 0x");
    puts_out(buf);
    putc_out('\n');
}

static void cmd_verify(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_out("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_VERIFY; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "ok\n" : "fail\n");
}

static void cmd_cd(ipc_queue_t *q, uint32_t self_id, const char *path) {
    (void)q; (void)self_id;
    char new[NOSFS_NAME_LEN];
    if (path[0] == '/' && path[1] == '\0') {
        cwd[0] = '\0';
        return;
    }
    build_path(path, new);
    size_t len = strlen(new);
    if (len && new[len-1] != '/') {
        if (len >= NOSFS_NAME_LEN-1) { puts_out("path too long\n"); return; }
        new[len] = '/'; new[len+1] = '\0';
    }
    if (find_handle(q, self_id, new) < 0) { puts_out("no such dir\n"); return; }
    strlcpy(cwd, new, sizeof(cwd));
}

static void cmd_mkdir(ipc_queue_t *q, uint32_t self_id, const char *name) {
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    size_t len = strlen(path);
    if (len >= NOSFS_NAME_LEN-1) { puts_out("name too long\n"); return; }
    if (path[len-1] != '/') { path[len] = '/'; path[len+1] = '\0'; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CREATE; msg.arg1 = 0; // zero capacity
    msg.arg2 = NOSFS_PERM_READ | NOSFS_PERM_WRITE;
    strlcpy((char *)msg.data, path, NOSFS_NAME_LEN);
    msg.len = strlen((char *)msg.data);
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "created\n" : "error\n");
}
static void cmd_install_pkg(ipc_queue_t *q, uint32_t self_id, const char *name) {
    ipc_message_t msg = {0}, reply = {0};
    size_t len = strlen(name);
    if (len > PKG_NAME_MAX - 1) len = PKG_NAME_MAX - 1;
    msg.type = PKG_MSG_INSTALL;
    memcpy(msg.data, name, len);
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "installed\n" : "error\n");
}
static void cmd_uninstall_pkg(ipc_queue_t *q, uint32_t self_id, const char *name) {
    ipc_message_t msg = {0}, reply = {0};
    size_t len = strlen(name);
    if (len > PKG_NAME_MAX - 1) len = PKG_NAME_MAX - 1;
    msg.type = PKG_MSG_UNINSTALL;
    memcpy(msg.data, name, len);
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "removed\n" : "error\n");
}
static void cmd_pkg_list(ipc_queue_t *q, uint32_t self_id) {
    ipc_message_t msg = {0}, reply = {0};
    msg.type = PKG_MSG_LIST;
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; ++i) {
        char *n = (char *)reply.data + i * PKG_NAME_MAX;
        puts_out(n);
        putc_out('\n');
    }
}
static void cmd_update(ipc_queue_t *q, uint32_t self_id, const char *target) {
    ipc_message_t msg = {0}, reply = {0};
    if (!strcmp(target, "kernel")) msg.type = UPDATE_MSG_KERNEL;
    else if (!strcmp(target, "userland")) msg.type = UPDATE_MSG_USERLAND;
    else { puts_out("unknown target\n"); return; }
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_out(reply.arg1 == 0 ? "updated\n" : "error\n");
}
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

// --- NitroShell main loop ---
void nsh_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id) {
    tty_clear();
    puts_out("NitroShell ready\n");
    puts_out("type 'help' for commands\n");
    char line[80]; char *argv[4];
    for (;;) {
        putc_out('>'); putc_out(' ');
        read_line(line, sizeof(line));
        int argc = tokenize(line, argv, 4);
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
        } else {
            puts_out("?\n");
        }
    }
}

/* ---- Logging macros: replace puts_out("...") with LOG_INFO("...") etc for color ----
#include "console.h"
#define LOG_INFO(msg)    vga_print("[INFO] " msg "\n", LOG_INFO)
#define LOG_ERROR(msg)   vga_print("[ERR!] " msg "\n", LOG_ERROR)
#define LOG_OK(msg)      vga_print("[ OK ] " msg "\n", LOG_OKAY)
*/


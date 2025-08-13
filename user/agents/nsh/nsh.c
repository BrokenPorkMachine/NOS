#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"

#define NOSFS_NAME_LEN 32

#include "../pkg/pkg.h"
#include "../update/update.h"
#include "../../include/nosfs.h"
#include "../nosfs/nosfs_server.h"
#include "../../../nosm/drivers/IO/tty.h"
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

static int fs_find_handle(ipc_queue_t *fs_q, uint32_t self_id, const char *path) {
    if (!fs_q)
        return -1;
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_LIST;
    if (ipc_send(fs_q, self_id, &msg) != 0)
        return -1;
    if (ipc_receive(fs_q, self_id, &reply) != 0)
        return -1;
    for (uint32_t i = 0; i < reply.arg1; ++i) {
        char *name = (char *)reply.data + i * NOSFS_NAME_LEN;
        if (strncmp(name, path, NOSFS_NAME_LEN) == 0)
            return (int)i;
    }
    return -1;
}

static void cmd_ls(ipc_queue_t *fs_q, uint32_t self_id) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_LIST;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0) {
        puts_out("ls error\n");
        return;
    }
    for (uint32_t i = 0; i < reply.arg1; ++i) {
        char *name = (char *)reply.data + i * NOSFS_NAME_LEN;
        puts_out(name);
        putc_out('\n');
    }
}

static void cmd_cat(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    int h = fs_find_handle(fs_q, self_id, path);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_READ;
    msg.arg1 = h;
    msg.arg2 = 0;
    msg.len = IPC_MSG_DATA_MAX;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || reply.arg1 != 0) {
        puts_out("read error\n");
        return;
    }
    for (uint32_t i = 0; i < reply.len; ++i)
        putc_out((char)reply.data[i]);
    putc_out('\n');
}

static void cmd_create(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CREATE;
    msg.arg1 = IPC_MSG_DATA_MAX;
    msg.arg2 = NOSFS_PERM_READ | NOSFS_PERM_WRITE;
    size_t len = strlen(path);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = (uint32_t)len;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || (int32_t)reply.arg1 < 0)
        puts_out("create failed\n");
}

static void cmd_write(ipc_queue_t *fs_q, uint32_t self_id,
                      const char *name, const char *data) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    int h = fs_find_handle(fs_q, self_id, path);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_WRITE;
    msg.arg1 = h;
    msg.arg2 = 0;
    size_t len = strlen(data);
    if (len > IPC_MSG_DATA_MAX)
        len = IPC_MSG_DATA_MAX;
    memcpy(msg.data, data, len);
    msg.len = (uint32_t)len;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("write failed\n");
}

static void cmd_rm(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    int h = fs_find_handle(fs_q, self_id, path);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_DELETE;
    msg.arg1 = h;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("rm failed\n");
}

static void cmd_mv(ipc_queue_t *fs_q, uint32_t self_id,
                   const char *oldn, const char *newn) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path_old[NOSFS_NAME_LEN], path_new[NOSFS_NAME_LEN];
    build_path(oldn, path_old);
    build_path(newn, path_new);
    int h = fs_find_handle(fs_q, self_id, path_old);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_RENAME;
    msg.arg1 = h;
    size_t len = strlen(path_new);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, path_new, len);
    msg.data[len] = '\0';
    msg.len = (uint32_t)len;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("mv failed\n");
}

static void cmd_crc(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    int h = fs_find_handle(fs_q, self_id, path);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CRC;
    msg.arg1 = h;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || (int32_t)reply.arg1 < 0) {
        puts_out("crc failed\n");
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%08x", reply.arg1);
    puts_out(buf);
    putc_out('\n');
}

static void cmd_verify(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    int h = fs_find_handle(fs_q, self_id, path);
    if (h < 0) {
        puts_out("not found\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_VERIFY;
    msg.arg1 = h;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0) {
        puts_out("verify failed\n");
        return;
    }
    puts_out(reply.arg1 == 0 ? "OK\n" : "BAD\n");
}

static void cmd_cd(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    (void)fs_q;
    (void)self_id;
    if (!strcmp(name, "/")) {
        strcpy(cwd, "/");
        return;
    }
    if (!strcmp(name, "..")) {
        char *p = strrchr(cwd, '/');
        if (p && p != cwd)
            *p = '\0';
        else
            strcpy(cwd, "/");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    if (path[0] == '\0') {
        strcpy(cwd, "/");
    } else {
        cwd[0] = '/';
        strncpy(cwd + 1, path, NOSFS_NAME_LEN - 2);
        cwd[NOSFS_NAME_LEN - 1] = '\0';
    }
}

static void cmd_mkdir(ipc_queue_t *fs_q, uint32_t self_id, const char *name) {
    if (!fs_q) {
        puts_out("filesystem unavailable\n");
        return;
    }
    char path[NOSFS_NAME_LEN];
    build_path(name, path);
    size_t len = strlen(path);
    if (len + 1 < sizeof(path) && path[len - 1] != '/') {
        path[len] = '/';
        path[len + 1] = '\0';
        len++;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NOSFS_MSG_CREATE;
    msg.arg1 = 0;
    msg.arg2 = NOSFS_PERM_READ | NOSFS_PERM_WRITE;
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = (uint32_t)len;
    if (ipc_send(fs_q, self_id, &msg) != 0 ||
        ipc_receive(fs_q, self_id, &reply) != 0 || (int32_t)reply.arg1 < 0)
        puts_out("mkdir failed\n");
}

static void cmd_install_pkg(ipc_queue_t *pkg_q, uint32_t self_id,
                            const char *name) {
    if (!pkg_q) {
        puts_out("pkg unavailable\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = PKG_MSG_INSTALL;
    size_t len = strlen(name);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, name, len);
    msg.data[len] = '\0';
    msg.len = (uint32_t)len;
    if (ipc_send(pkg_q, self_id, &msg) != 0 ||
        ipc_receive(pkg_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("install failed\n");
}

static void cmd_uninstall_pkg(ipc_queue_t *pkg_q, uint32_t self_id,
                              const char *name) {
    if (!pkg_q) {
        puts_out("pkg unavailable\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = PKG_MSG_UNINSTALL;
    size_t len = strlen(name);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    memcpy(msg.data, name, len);
    msg.data[len] = '\0';
    msg.len = (uint32_t)len;
    if (ipc_send(pkg_q, self_id, &msg) != 0 ||
        ipc_receive(pkg_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("uninstall failed\n");
}

static void cmd_pkg_list(ipc_queue_t *pkg_q, uint32_t self_id) {
    if (!pkg_q) {
        puts_out("pkg unavailable\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = PKG_MSG_LIST;
    if (ipc_send(pkg_q, self_id, &msg) != 0 ||
        ipc_receive(pkg_q, self_id, &reply) != 0) {
        puts_out("pkglist failed\n");
        return;
    }
    for (uint32_t i = 0; i < reply.arg1; ++i) {
        char *name = (char *)reply.data + i * PKG_NAME_MAX;
        puts_out(name);
        putc_out('\n');
    }
}

static void cmd_update(ipc_queue_t *upd_q, uint32_t self_id, const char *target) {
    if (!upd_q) {
        puts_out("update unavailable\n");
        return;
    }
    uint32_t type = 0;
    if (!strcmp(target, "kernel"))
        type = UPDATE_MSG_KERNEL;
    else if (!strcmp(target, "userland"))
        type = UPDATE_MSG_USERLAND;
    else {
        puts_out("unknown target\n");
        return;
    }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = type;
    if (ipc_send(upd_q, self_id, &msg) != 0 ||
        ipc_receive(upd_q, self_id, &reply) != 0 || reply.arg1 != 0)
        puts_out("update failed\n");
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
    puts_out("  pwd       - print working directory\n");
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
        } else if (!strcmp(argv[0], "pwd")) {
            puts_out(cwd);
            putc_out('\n');
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

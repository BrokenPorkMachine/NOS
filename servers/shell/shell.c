#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "../nitrfs/nitrfs.h"
#include "../nitrfs/server.h"
#include "../../IO/keyboard.h"
#include "../../IO/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Optional: #include "console.h" for log colors (see LOG_* macros at bottom)

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;

static void vga_clear_screen(void) {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (0x0F << 8) | ' ';
    row = 0;
    col = 0;
}

static void vga_scroll() {
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int r = 1; r < VGA_ROWS-1; ++r)
        for (int c = 0; c < VGA_COLS; ++c)
            vga[r*VGA_COLS + c] = vga[(r+1)*VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; ++c)
        vga[(VGA_ROWS-1)*VGA_COLS + c] = (0x0F << 8) | ' ';
    if (row > 1) row--;
}

static void putc_vga(char c) {
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
    if (c == '\n') {
        col = 0;
        if (++row >= VGA_ROWS - 1) {
            vga_scroll();
            row = VGA_ROWS - 2;
        }
        return;
    }
    vga[row * VGA_COLS + col] = (0x0F << 8) | c;
    if (++col >= VGA_COLS) {
        col = 0;
        if (++row >= VGA_ROWS - 1) {
            vga_scroll();
            row = VGA_ROWS - 2;
        }
    }
}

static void puts_vga(const char *s) { while (*s) putc_vga(*s++); }

static char cwd[NITRFS_NAME_LEN] = "";

static void build_path(const char *name, char out[NITRFS_NAME_LEN]) {
    if (name[0] == '/') {
        strncpy(out, name + 1, NITRFS_NAME_LEN - 1);
        out[NITRFS_NAME_LEN - 1] = '\0';
        return;
    }
    size_t len = strlen(cwd);
    strncpy(out, cwd, NITRFS_NAME_LEN - 1);
    out[NITRFS_NAME_LEN - 1] = '\0';
    if (len && len < NITRFS_NAME_LEN - 1)
        out[len++] = '/';
    strncpy(out + len, name, NITRFS_NAME_LEN - len - 1);
    out[NITRFS_NAME_LEN - 1] = '\0';
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
        ch = keyboard_getchar();
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
            putc_vga('\n');
            break;
        }
        if (c == '\b' || c == 127) { // Backspace
            if (pos > 0) {
                putc_vga('\b');
                putc_vga(' ');
                putc_vga('\b');
                pos--;
            }
            continue;
        }
        if (pos + 1 < len && c) {
            buf[pos++] = c;
            putc_vga(c);
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
    char path[NITRFS_NAME_LEN];
    build_path(name, path);
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_LIST;
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        char *n = (char *)reply.data + i * NITRFS_NAME_LEN;
        if (strncmp(n, path, NITRFS_NAME_LEN) == 0)
            return i;
    }
    return -1;
}

// --- Commands ---
static void cmd_ls(ipc_queue_t *q, uint32_t self_id) {
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_LIST; msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        const char *n = (char *)reply.data + i * NITRFS_NAME_LEN;
        size_t pref = strlen(cwd);
        if (pref && strncmp(n, cwd, pref) != 0)
            continue;
        if (pref && n[pref] == '/')
            n += pref + 1;
        puts_vga(n);
        putc_vga('\n');
    }
}
static void cmd_cat(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_READ; msg.arg1 = h; msg.arg2 = IPC_MSG_DATA_MAX;
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    if (reply.arg1 != 0) { puts_vga("read error\n"); return; }
    reply.data[reply.len] = '\0';
    puts_vga((char *)reply.data);
    putc_vga('\n');
}
static void cmd_create(ipc_queue_t *q, uint32_t self_id, const char *name) {
    char path[NITRFS_NAME_LEN];
    build_path(name, path);
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_CREATE; msg.arg1 = IPC_MSG_DATA_MAX;
    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
    size_t len = strlen(path);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    strncpy((char *)msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "created\n" : "error\n");
}
static void cmd_write(ipc_queue_t *q, uint32_t self_id, const char *name, const char *data) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    size_t len = strlen(data);
    if (len > IPC_MSG_DATA_MAX) len = IPC_MSG_DATA_MAX;
    msg.type = NITRFS_MSG_WRITE; msg.arg1 = h; msg.arg2 = len;
    memcpy(msg.data, data, len); msg.len = len;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "ok\n" : "error\n");
}
static void cmd_rm(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_DELETE; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "deleted\n" : "error\n");
}
static void cmd_mv(ipc_queue_t *q, uint32_t self_id, const char *old, const char *new) {
    int h = find_handle(q, self_id, old);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_RENAME;
    msg.arg1 = h;
    char path[NITRFS_NAME_LEN];
    build_path(new, path);
    size_t len = strlen(path);
    if (len > IPC_MSG_DATA_MAX - 1)
        len = IPC_MSG_DATA_MAX - 1;
    strncpy((char *)msg.data, path, len);
    msg.data[len] = '\0';
    msg.len = len;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "moved\n" : "error\n");
}

static void cmd_crc(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_CRC; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    if (reply.arg1 != 0) { puts_vga("error\n"); return; }
    char buf[9];
    u32_hex(reply.arg2, buf);
    puts_vga("crc: 0x");
    puts_vga(buf);
    putc_vga('\n');
}

static void cmd_verify(ipc_queue_t *q, uint32_t self_id, const char *name) {
    int h = find_handle(q, self_id, name);
    if (h < 0) { puts_vga("not found\n"); return; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_VERIFY; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "ok\n" : "fail\n");
}

static void cmd_cd(ipc_queue_t *q, uint32_t self_id, const char *path) {
    (void)q; (void)self_id;
    char new[NITRFS_NAME_LEN];
    if (path[0] == '/' && path[1] == '\0') {
        cwd[0] = '\0';
        return;
    }
    build_path(path, new);
    size_t len = strlen(new);
    if (len && new[len-1] != '/') {
        if (len >= NITRFS_NAME_LEN-1) { puts_vga("path too long\n"); return; }
        new[len] = '/'; new[len+1] = '\0';
    }
    if (find_handle(q, self_id, new) < 0) { puts_vga("no such dir\n"); return; }
    strlcpy(cwd, new, sizeof(cwd));
}

static void cmd_mkdir(ipc_queue_t *q, uint32_t self_id, const char *name) {
    char path[NITRFS_NAME_LEN];
    build_path(name, path);
    size_t len = strlen(path);
    if (len >= NITRFS_NAME_LEN-1) { puts_vga("name too long\n"); return; }
    if (path[len-1] != '/') { path[len] = '/'; path[len+1] = '\0'; }
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_CREATE; msg.arg1 = 0; // zero capacity
    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
    strlcpy((char *)msg.data, path, NITRFS_NAME_LEN);
    msg.len = strlen((char *)msg.data);
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 == 0 ? "created\n" : "error\n");
}
static void cmd_help(void) {
    puts_vga("Available commands:\n");
    puts_vga("  ls        - list files\n");
    puts_vga("  cat FILE  - display file contents\n");
    puts_vga("  create FILE - create a new file\n");
    puts_vga("  write FILE DATA - write data to file\n");
    puts_vga("  rm FILE   - delete a file\n");
    puts_vga("  mv OLD NEW - rename file\n");
    puts_vga("  crc FILE  - compute CRC32\n");
    puts_vga("  verify FILE - verify CRC32\n");
    puts_vga("  cd DIR    - change directory\n");
    puts_vga("  mkdir DIR - make directory\n");
    puts_vga("  help      - show this message\n");
}

// --- Shell main loop ---
void shell_main(ipc_queue_t *q, uint32_t self_id) {
    vga_clear_screen();
    serial_puts("[shell] starting\n");
    puts_vga("NOS shell ready\n");
    puts_vga("type 'help' for commands\n");
    char line[80]; char *argv[4];
    for (;;) {
        putc_vga('>'); putc_vga(' ');
        read_line(line, sizeof(line));
        int argc = tokenize(line, argv, 4);
        if (argc == 0) continue;
        if (!strcmp(argv[0], "ls") || !strcmp(argv[0], "dir")) {
            cmd_ls(q, self_id);
        } else if (!strcmp(argv[0], "cat") && argc > 1) {
            cmd_cat(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "create") && argc > 1) {
            cmd_create(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "write") && argc > 2) {
            cmd_write(q, self_id, argv[1], argv[2]);
        } else if (!strcmp(argv[0], "rm") && argc > 1) {
            cmd_rm(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "mv") && argc > 2) {
            cmd_mv(q, self_id, argv[1], argv[2]);
        } else if (!strcmp(argv[0], "crc") && argc > 1) {
            cmd_crc(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "verify") && argc > 1) {
            cmd_verify(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "cd") && argc > 1) {
            cmd_cd(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "mkdir") && argc > 1) {
            cmd_mkdir(q, self_id, argv[1]);
        } else if (!strcmp(argv[0], "help")) {
            cmd_help();
        } else {
            puts_vga("?\n");
        }
    }
}

/* ---- Logging macros: replace puts_vga("...") with LOG_INFO("...") etc for color ----
#include "console.h"
#define LOG_INFO(msg)    vga_print("[INFO] " msg "\n", LOG_INFO)
#define LOG_ERROR(msg)   vga_print("[ERR!] " msg "\n", LOG_ERROR)
#define LOG_OK(msg)      vga_print("[ OK ] " msg "\n", LOG_OKAY)
*/


#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "../nitrfs/nitrfs.h"
#include "../nitrfs/server.h"
#include "../../IO/keyboard.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Optional: #include "console.h" for log colors (see LOG_* macros at bottom)

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 12, col = 0;

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

static const char keymap[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',[23]='i',[24]='o',[25]='p',
    [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',
    [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',
    [57]=' '
};
static char scancode_to_ascii(uint8_t sc) { return (sc < 128) ? keymap[sc] : 0; }

static inline void sys_yield(void) { asm volatile("mov $0, %%rax; int $0x80" ::: "rax"); }

// --- Input (line editing, supports backspace) ---
static char getchar_block(void) {
    int sc;
    char c = 0;
    while (!c) {
        sc = keyboard_read_scancode();
        if (sc >= 0 && !(sc & 0x80))
            c = scancode_to_ascii(sc);
        if (!c)
            sys_yield();
    }
    return c;
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
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_LIST;
    msg.len = 0;
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    for (int i = 0; i < (int)reply.arg1; i++) {
        char *n = (char *)reply.data + i * NITRFS_NAME_LEN;
        if (strncmp(n, name, NITRFS_NAME_LEN) == 0)
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
        puts_vga((char *)reply.data + i * NITRFS_NAME_LEN);
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
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_CREATE; msg.arg1 = IPC_MSG_DATA_MAX;
    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
    strncpy((char *)msg.data, name, IPC_MSG_DATA_MAX); msg.len = strlen(name);
    ipc_send(q, self_id, &msg);
    ipc_receive(q, self_id, &reply);
    puts_vga(reply.arg1 >= 0 ? "created\n" : "error\n");
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
    msg.type = NITRFS_MSG_CREATE; msg.arg1 = IPC_MSG_DATA_MAX;
    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
    strncpy((char *)msg.data, new, IPC_MSG_DATA_MAX); msg.len = strlen(new);
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    int new_h = reply.arg1;
    if (new_h < 0) { puts_vga("error\n"); return; }
    msg.type = NITRFS_MSG_READ; msg.arg1 = h; msg.arg2 = IPC_MSG_DATA_MAX; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    if (reply.arg1 != 0) { puts_vga("error\n"); return; }
    uint32_t len = reply.len;
    msg.type = NITRFS_MSG_WRITE; msg.arg1 = new_h; msg.arg2 = len;
    memcpy(msg.data, reply.data, len); msg.len = len;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    msg.type = NITRFS_MSG_DELETE; msg.arg1 = h; msg.len = 0;
    ipc_send(q, self_id, &msg); ipc_receive(q, self_id, &reply);
    puts_vga("moved\n");
}
static void cmd_help(void) {
    puts_vga("Available commands:\n");
    puts_vga("  ls        - list files\n");
    puts_vga("  cat FILE  - display file contents\n");
    puts_vga("  create FILE - create a new file\n");
    puts_vga("  write FILE DATA - write data to file\n");
    puts_vga("  rm FILE   - delete a file\n");
    puts_vga("  mv OLD NEW - rename file\n");
    puts_vga("  cd DIR    - change directory (not implemented)\n");
    puts_vga("  mkdir DIR - make directory (not implemented)\n");
    puts_vga("  help      - show this message\n");
}

// --- Shell main loop ---
void shell_main(ipc_queue_t *q, uint32_t self_id) {
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
        } else if (!strcmp(argv[0], "cd") || !strcmp(argv[0], "mkdir")) {
            puts_vga("not implemented\n");
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


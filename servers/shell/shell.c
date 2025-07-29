#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "../nitrfs/nitrfs.h"
#include "../../Task/thread.h"

#define VGA_TEXT_BUF 0xB8000
static int row = 12; // start lower on the screen

static void puts_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
    int col = 0;
    while (s[col]) {
        vga[row * 80 + col] = (0x0F << 8) | s[col];
        col++;
    }
    row++;
}

enum {
    NITRFS_MSG_CREATE = 1,
    NITRFS_MSG_WRITE,
    NITRFS_MSG_READ,
    NITRFS_MSG_DELETE,
    NITRFS_MSG_LIST
};

void shell_main(ipc_queue_t *q) {
    ipc_message_t msg, reply;
    const char *fname = "hello.txt";
    const char *content = "Hello from shell";

    puts_vga("[shell] create file");
    msg.type = NITRFS_MSG_CREATE;
    msg.arg1 = 128;
    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
    memset(msg.data, 0, IPC_MSG_DATA_MAX);
    memcpy(msg.data, fname, strlen(fname)+1);
    ipc_send(q, &msg);
    ipc_receive(q, &reply);
    int handle = reply.arg1;

    puts_vga("[shell] write file");
    msg.type = NITRFS_MSG_WRITE;
    msg.arg1 = handle;
    msg.arg2 = strlen(content);
    memset(msg.data, 0, IPC_MSG_DATA_MAX);
    memcpy(msg.data, content, msg.arg2);
    ipc_send(q, &msg);
    ipc_receive(q, &reply);

    puts_vga("[shell] list files");
    msg.type = NITRFS_MSG_LIST;
    ipc_send(q, &msg);
    ipc_receive(q, &reply);
    int count = reply.arg1;
    for (int i = 0; i < count; ++i) {
        puts_vga((char *)reply.data + i * NITRFS_NAME_LEN);
    }

    while (1) {
        // Halt this thread
        for (volatile int i = 0; i < 1000000; ++i);
        schedule();
    }
}

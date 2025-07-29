#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "../nitrfs/nitrfs.h"
#include "../nitrfs/server.h"
#include "../../Task/thread.h"
#include "../../IO/keyboard.h"
#include <stdint.h>

#define VGA_TEXT_BUF 0xB8000
static int row = 12;
static int col = 0;

static void putc_vga(char c) {
    volatile uint16_t *vga = (uint16_t *)VGA_TEXT_BUF;
    if (c == '\n') {
        col = 0;
        row++;
        return;
    }
    vga[row * 80 + col] = (0x0F << 8) | c;
    col++;
    if (col >= 80) { col = 0; row++; }
}

static void puts_vga(const char *s) {
    while (*s) putc_vga(*s++);
}

static const char keymap[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',[23]='i',[24]='o',[25]='p',
    [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',
    [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',
    [57]=' '
};

static char scancode_to_ascii(uint8_t sc) {
    if (sc < 128) return keymap[sc];
    return 0;
}

static char getchar_block(void) {
    int sc;
    char c = 0;
    while (!c) {
        sc = keyboard_read_scancode();
        if (sc >= 0 && !(sc & 0x80))
            c = scancode_to_ascii(sc);
        if (!c)
            thread_yield();
    }
    return c;
}

void shell_main(ipc_queue_t *q) {
    ipc_message_t msg, reply;
    int handle = -1;

    puts_vga("NOS shell ready\n");
    puts_vga("1:create 2:write 3:read 4:list 5:crc 6:verify\n");

    while (1) {
        putc_vga('>'); putc_vga(' ');
        char cmd = getchar_block();
        putc_vga(cmd); putc_vga('\n');
        msg.len = 0;

        switch (cmd) {
        case '1':
            msg.type = NITRFS_MSG_CREATE;
            msg.arg1 = 128;
            msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
            strncpy((char *)msg.data, "file.txt", IPC_MSG_DATA_MAX);
            msg.len = strlen("file.txt");
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            handle = reply.arg1;
            puts_vga("created\n");
            break;
        case '2':
            if (handle < 0) { puts_vga("no file\n"); break; }
            msg.type = NITRFS_MSG_WRITE;
            msg.arg1 = handle;
            strncpy((char *)msg.data, "data", IPC_MSG_DATA_MAX);
            msg.arg2 = 4;
            msg.len  = 4;
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            puts_vga("wrote\n");
            break;
        case '3':
            if (handle < 0) { puts_vga("no file\n"); break; }
            msg.type = NITRFS_MSG_READ;
            msg.arg1 = handle;
            msg.arg2 = IPC_MSG_DATA_MAX;
            msg.len  = 0;
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            if (reply.arg1 == 0) {
                reply.data[reply.len] = '\0';
                puts_vga((char *)reply.data);
                putc_vga('\n');
            }
            break;
        case '4':
            msg.type = NITRFS_MSG_LIST;
            msg.len  = 0;
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            for (int i=0;i<reply.arg1;i++) {
                puts_vga((char *)reply.data + i*NITRFS_NAME_LEN);
                putc_vga('\n');
            }
            break;
        case '5':
            if (handle < 0) { puts_vga("no file\n"); break; }
            msg.type = NITRFS_MSG_CRC;
            msg.arg1 = handle;
            msg.len  = 0;
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            if (reply.arg1==0) {
                puts_vga("crc ok\n");
            } else {
                puts_vga("crc fail\n");
            }
            break;
        case '6':
            if (handle < 0) { puts_vga("no file\n"); break; }
            msg.type = NITRFS_MSG_VERIFY;
            msg.arg1 = handle;
            msg.len  = 0;
            ipc_send(q, &msg);
            ipc_receive(q, &reply);
            puts_vga(reply.arg1==0?"verify ok\n":"verify bad\n");
            break;
        default:
            puts_vga("?\n");
        }
    }
}

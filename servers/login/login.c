#include "login.h"
#include "../../IO/keyboard.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../src/libc.h"
#include <stddef.h>

volatile int login_done = 0;

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;

static void putc_vga(char c)
{
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    if(c=='\n') {
        col = 0;
        if(++row >= VGA_ROWS-1) row = VGA_ROWS-2;
        return;
    }
    vga[row*VGA_COLS + col] = (0x0F << 8) | c;
    if(++col >= VGA_COLS) { col = 0; if(++row >= VGA_ROWS-1) row = VGA_ROWS-2; }
}

static void puts_out(const char *s)
{
    while(*s) { serial_write(*s); putc_vga(*s++); }
}

static char getchar_block(void)
{
    int ch = -1;
    while((ch = keyboard_getchar()) < 0)
        thread_yield();
    return (char)ch;
}

static void read_line(char *buf, size_t len, int hide)
{
    size_t pos=0;
    for(;;) {
        char c = getchar_block();
        if(c=='\n' || c=='\r') { puts_out("\n"); break; }
        if((c=='\b' || c==127) && pos>0) {
            puts_out("\b \b");
            pos--; continue;
        }
        if(pos+1 < len && c) {
            buf[pos++] = c;
            if(hide) {
                puts_out("*");
            } else {
                char str[2]={c,0};
                puts_out(str);
            }
        }
    }
    buf[pos]=0;
}

void login_server(ipc_queue_t *q, uint32_t self_id)
{
    (void)q; (void)self_id;
    serial_puts("[login] login server starting\n");
    char user[32];
    char pass[32];
    for(;;) {
        puts_out("Username: ");
        read_line(user, sizeof(user), 0);
        puts_out("Password: ");
        read_line(pass, sizeof(pass), 1);
        if(!strcmp(user,"admin") && !strcmp(pass,"admin")) {
            puts_out("Login successful\n");
            login_done = 1;
            break;
        } else {
            puts_out("Login failed\n");
        }
    }
    serial_puts("[login] exiting\n");
}

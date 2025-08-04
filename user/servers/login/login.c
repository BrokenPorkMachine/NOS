#include "login.h"
#include "../../../kernel/drivers/IO/keyboard.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/drivers/IO/video.h"
#include "../../../kernel/Task/thread.h"
#include "../../libc/libc.h"
#include "font8x8_basic.h"
#include <stddef.h>

volatile int login_done = 0;
volatile login_session_t current_session = {0};

typedef struct {
    const char *user;
    const char *pass;
    uint32_t uid;
} credential_t;

static const credential_t cred_store[] = {
    {"admin", "admin", 0},
    {"guest", "guest", 1},
};
static const size_t cred_count = sizeof(cred_store)/sizeof(cred_store[0]);

static int authenticate(const char *user, const char *pass, const credential_t **out)
{
    for (size_t i = 0; i < cred_count; ++i) {
        if (!strcmp(user, cred_store[i].user) && !strcmp(pass, cred_store[i].pass)) {
            if (out) *out = &cred_store[i];
            return 0;
        }
    }
    return -1;
}

#define VGA_TEXT_BUF 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static int row = 0, col = 0;
static int fb_row = 0, fb_col = 0;

static void clear_screen(void)
{
    video_clear(0); /* Clear framebuffer to black */
    volatile uint16_t *vga = (uint16_t*)VGA_TEXT_BUF;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; ++i)
        vga[i] = (0x0F << 8) | ' ';
    row = col = 0;
    fb_row = fb_col = 0;
}

static void draw_char_fb(uint32_t x, uint32_t y, char c)
{
    const uint8_t *glyph = font8x8_basic[(unsigned char)c];
    for (int yy = 0; yy < 8; ++yy) {
        for (int xx = 0; xx < 8; ++xx) {
            uint32_t color = (glyph[yy] & (1 << xx)) ? 0xFFFFFFFF : 0x000000;
            video_draw_pixel(x + xx, y + yy, color);
        }
    }
}

static void putc_console(char c)
{
    const bootinfo_framebuffer_t *fb = video_get_info();
    if (fb && fb->address) {
        if (c == '\n') { fb_col = 0; fb_row++; return; }
        draw_char_fb(fb_col * 8, fb_row * 8, c);
        if (++fb_col >= (int)(fb->width / 8)) { fb_col = 0; fb_row++; }
        return;
    }
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
    serial_puts(s);
    while(*s) { putc_console(*s++); }
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
    clear_screen();
    puts_out("[login] login server starting\n");
    /* Give other threads a chance to run so the start message is visible */
    thread_yield();
    char user[32];
    char pass[32];
    for(;;) {
        const credential_t *cred = NULL;
        puts_out("Username: ");
        read_line(user, sizeof(user), 0);
        puts_out("Password: ");
        read_line(pass, sizeof(pass), 1);
        if(authenticate(user, pass, &cred) == 0) {
            puts_out("Login successful\n");
            current_session.uid = cred->uid;
            strncpy((char*)current_session.username, cred->user, sizeof(current_session.username)-1);
            current_session.session_id++;
            current_session.active = 1;
            login_done = 1;
            break;
        } else {
            puts_out("Login failed\n");
        }
    }
    serial_puts("[login] exiting\n");
}

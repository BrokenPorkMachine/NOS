#ifndef TTY_H
#define TTY_H

#include <stdint.h>

void tty_init(void);
void tty_enable_framebuffer(int enable);
void tty_clear(void);
void tty_use_vga(int enable);
void tty_putc(char c);
/* Output to the display without serial logging. */
void tty_putc_noserial(char c);
void tty_write(const char *s);
int tty_getchar(void);

#endif // TTY_H

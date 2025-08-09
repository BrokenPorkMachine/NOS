#ifndef TTY_H
#define TTY_H

#include <stdint.h>

void tty_init(void);
void tty_clear(void);
void tty_putc(char c);
void tty_write(const char *s);
int tty_getchar(void);

#endif // TTY_H

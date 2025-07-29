#include "syscall.h"
#include "../Task/thread.h"
#include <stdint.h>

static uint64_t sys_write_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80 * 6; // row 6
    while (*s) {
        *vga++ = (0x0F << 8) | *s++;
    }
    return 0;
}

uint64_t syscall_handle(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2; (void)arg3;
    switch (num) {
    case SYS_YIELD:
        thread_yield();
        return 0;
    case SYS_WRITE_VGA:
        return sys_write_vga((const char *)arg1);
    default:
        return (uint64_t)-1;
    }
}

#include <stddef.h>
#include <stdint.h>
#include "klib/string.h"
#include "drivers/IO/tty.h"
#include "syscall.h"

#define SYS_OPEN  8
#define SYS_READ  9
#define SYS_WRITE 10
#define SYS_CLOSE 11

#define MAX_DEVICES 8

typedef long (*dev_read_fn)(void *buf, size_t n);
typedef long (*dev_write_fn)(const void *buf, size_t n);

typedef struct {
    const char   *name;
    dev_read_fn   read;
    dev_write_fn  write;
} device_t;

static device_t devices[MAX_DEVICES];
static int dev_count = 0;

static int dev_lookup(const char *name) {
    for (int i = 0; i < dev_count; ++i) {
        if (strcmp(name, devices[i].name) == 0)
            return i;
    }
    return -1;
}

static int dev_register(const char *name, dev_read_fn r, dev_write_fn w) {
    if (dev_count >= MAX_DEVICES)
        return -1;
    devices[dev_count].name  = name;
    devices[dev_count].read  = r;
    devices[dev_count].write = w;
    return dev_count++;
}

static long console_write(const void *buf, size_t n) {
    const char *p = (const char *)buf;
    char tmp[256];
    size_t off = 0;
    while (off < n) {
        size_t chunk = n - off;
        if (chunk >= sizeof(tmp))
            chunk = sizeof(tmp) - 1;
        memcpy(tmp, p + off, chunk);
        tmp[chunk] = '\0';
        tty_write(tmp);
        off += chunk;
    }
    return (long)n;
}

static long console_read(void *buf, size_t n) {
    char *p = (char *)buf;
    size_t i = 0;
    for (; i < n; ++i) {
        int ch = tty_getchar();
        if (ch < 0)
            break;
        p[i] = (char)ch;
    }
    return (long)i;
}

void devfs_init(void) {
    dev_count = 0;
    dev_register("console", console_read, console_write);
}

static long sys_open(const char *path) {
    if (!path)
        return -1;
    if (strncmp(path, "/dev/", 5) == 0) {
        int idx = dev_lookup(path + 5);
        return idx >= 0 ? idx : -1;
    }
    return -1;
}

static long sys_write_fd(long fd, const void *buf, size_t n) {
    if (fd < 0 || fd >= dev_count)
        return -1;
    if (!devices[fd].write)
        return -1;
    return devices[fd].write(buf, n);
}

static long sys_read_fd(long fd, void *buf, size_t n) {
    if (fd < 0 || fd >= dev_count)
        return 0;
    if (!devices[fd].read)
        return 0;
    return devices[fd].read(buf, n);
}

long isr_syscall_handler(syscall_regs_t *regs) {
    switch (regs->rax) {
    case SYS_OPEN:
        return sys_open((const char *)regs->rdi);
    case SYS_WRITE:
        return sys_write_fd((long)regs->rdi, (const void *)regs->rsi, (size_t)regs->rdx);
    case SYS_READ:
        return sys_read_fd((long)regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
    case SYS_CLOSE:
        return 0;
    default:
        return -1;
    }
}

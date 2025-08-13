#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdatomic.h>
#include "../user/libc/libc.h"
#include "nosfs.h"
extern unsigned char init_bin[];
extern unsigned int init_bin_len;
extern unsigned char login_bin[];
extern unsigned int login_bin_len;

/* Basic kernel logging helper */
extern int kprintf(const char *fmt, ...);

/* Yield stub so busy loops don't hog the CPU */
extern void thread_yield(void);

typedef struct ipc_queue ipc_queue_t;

// Stub build always considers filesystem ready
_Atomic int nosfs_ready = 1;
nosfs_fs_t nosfs_root;

int nosfs_is_ready(void) { return nosfs_ready; }
int nosfs_create(nosfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm) {
    (void)fs; (void)name; (void)capacity; (void)perm; return -1;
}
int nosfs_write(nosfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    (void)fs; (void)handle; (void)offset; (void)buf; (void)len; return -1;
}

/* Miscellaneous stubs required for linking without full hardware support */
void *alloc_thread_struct(void) { return NULL; }
void *alloc_stack(size_t size, int user_mode) { (void)size; (void)user_mode; return NULL; }
unsigned char init_bin[] = {0};
unsigned int init_bin_len = 0;
unsigned char login_bin[] = {0};
unsigned int login_bin_len = 0;

void usb_init(void) {}
void usb_kbd_init(void) {}
void video_init(const void *fb) { (void)fb; }
void tty_init(void) {}
void ps2_init(void) {}
void block_init(void) {}
int  sata_init(void) { return 0; }
void net_init(void) {}

/* Minimal TTY hook so login/nsh can print messages */
void tty_write(const char *s) { kprintf("%s", s); }

int block_read(uint32_t lba, uint8_t *buf, size_t count) { (void)lba; (void)buf; (void)count; return -1; }
int block_write(uint32_t lba, const uint8_t *buf, size_t count) { (void)lba; (void)buf; (void)count; return -1; }

int fs_read_all(const char *path, void **out, size_t *out_sz) {
    if (!path || !out || !out_sz)
        return -1;
    if (strcmp(path, "/agents/init.mo2") == 0) {
        *out = (void *)init_bin;
        *out_sz = init_bin_len;
        return 0;
    }
    if (strcmp(path, "/agents/login.mo2") == 0) {
        *out = (void *)login_bin;
        *out_sz = login_bin_len;
        return 0;
    }
    /* No real filesystem; return not found */
    *out = NULL;
    *out_sz = 0;
    return -1;
}

/* Stubbed core agents so boot shows progress */
void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    kprintf("[nosfs] stub filesystem agent online\n");
    for (;;) thread_yield();
}

void nosm_entry(void) {
    kprintf("[nosm] stub module loader online\n");
    for (;;) thread_yield();
}


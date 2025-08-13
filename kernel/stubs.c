#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdatomic.h>
#include "../user/libc/libc.h"
#include "nosfs.h"
// libc.h already provides malloc/str* prototypes and time.
extern unsigned char init_bin[];
extern unsigned int init_bin_len;
extern unsigned char login_bin[];
extern unsigned int login_bin_len;

/* Basic kernel logging helper */
extern int kprintf(const char *fmt, ...);

/* Yield stub so busy loops don't hog the CPU */
extern void thread_yield(void);

typedef struct ipc_queue ipc_queue_t;

// In-memory NOSFS used by the kernel.  Initialised by nosfs_server().
_Atomic int nosfs_ready = 0;
nosfs_fs_t nosfs_root;

int nosfs_is_ready(void) { return atomic_load(&nosfs_ready); }

static time_t nosfs_time(void) {
    static time_t cur = 0;
    return cur++;
}

void nosfs_init(nosfs_fs_t *fs) {
    memset(fs, 0, sizeof(*fs));
    pthread_mutex_init(&fs->mutex, NULL);
    atomic_store(&nosfs_ready, 1);
}

static nosfs_file_t *nosfs_get(nosfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return NULL;
    return &fs->files[handle];
}

int nosfs_create(nosfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm) {
    if (!fs || !name || !capacity)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    if (fs->file_count >= NOSFS_MAX_FILES) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        if (strncmp(fs->files[i].name, name, NOSFS_NAME_LEN) == 0) {
            pthread_mutex_unlock(&fs->mutex);
            return -1;
        }
    }
    nosfs_file_t *f = &fs->files[fs->file_count];
    strncpy(f->name, name, NOSFS_NAME_LEN - 1);
    f->name[NOSFS_NAME_LEN - 1] = '\0';
    f->data = calloc(1, capacity);
    if (!f->data) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
    f->size = 0;
    f->capacity = capacity;
    f->perm = perm;
    f->created_at = f->modified_at = nosfs_time();
    int handle = fs->file_count++;
    pthread_mutex_unlock(&fs->mutex);
    return handle;
}

int nosfs_write(nosfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    if (!fs || !buf)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nosfs_file_t *f = nosfs_get(fs, handle);
    if (!f || offset + len > f->capacity) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
    memcpy(f->data + offset, buf, len);
    if (offset + len > f->size)
        f->size = offset + len;
    f->modified_at = nosfs_time();
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nosfs_read(nosfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len) {
    if (!fs || !buf)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nosfs_file_t *f = nosfs_get(fs, handle);
    if (!f || offset + len > f->size) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
    memcpy(buf, f->data + offset, len);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

/* Miscellaneous stubs required for linking without full hardware support */
extern size_t thread_struct_size;

void *alloc_thread_struct(void) {
    return calloc(1, thread_struct_size);
}

void *alloc_stack(size_t size, int user_mode) {
    (void)user_mode;
    uint8_t *mem = malloc(size);
    if (!mem)
        return NULL;
    return mem + size;
}
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

    const char *name = path;
    if (name[0] == '/')
        name++;

    pthread_mutex_lock(&nosfs_root.mutex);
    for (size_t i = 0; i < nosfs_root.file_count; ++i) {
        if (strncmp(nosfs_root.files[i].name, name, NOSFS_NAME_LEN) == 0) {
            nosfs_file_t *f = &nosfs_root.files[i];
            void *buf = malloc(f->size);
            if (!buf) {
                pthread_mutex_unlock(&nosfs_root.mutex);
                return -1;
            }
            memcpy(buf, f->data, f->size);
            *out = buf;
            *out_sz = f->size;
            pthread_mutex_unlock(&nosfs_root.mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&nosfs_root.mutex);
    *out = NULL;
    *out_sz = 0;
    return -1;
}

/* Stubbed core agents so boot shows progress */
void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    nosfs_init(&nosfs_root);
    kprintf("[nosfs] in-memory filesystem ready\n");
    for (;;) thread_yield();
}

void nosm_entry(void) {
    kprintf("[nosm] module loader idle\n");
    for (;;) thread_yield();
}


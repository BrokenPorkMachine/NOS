#ifndef NITRFS_H
#define NITRFS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef KERNEL_BUILD
typedef struct {
    volatile int lock;
    uint32_t owner;
    int count;
} pthread_mutex_t;
typedef void* pthread_mutexattr_t;
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
#else
#include <pthread.h>
#endif


#define NITRFS_MAX_FILES    16
#define NITRFS_NAME_LEN     32
#define NITRFS_BLOCK_SIZE   512
#define NITRFS_ACL_MAX      8
#define NITRFS_MAGIC        0x4E495452  // 'NITR'
#define NITRFS_PERM_READ    0x1
#define NITRFS_PERM_WRITE   0x2

typedef struct {
    uint32_t uid;
    uint32_t perm;
} nitrfs_acl_entry_t;

typedef struct {
    char     name[NITRFS_NAME_LEN];
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
    uint32_t perm;
    uint32_t owner;
    uint32_t crc32;
    size_t   acl_count;
    nitrfs_acl_entry_t acl[NITRFS_ACL_MAX];
    time_t   created_at;
    time_t   modified_at;
} nitrfs_file_t;

typedef struct {
    nitrfs_file_t files[NITRFS_MAX_FILES];
    size_t file_count;
    pthread_mutex_t mutex;
    uint32_t max_files;
    uint32_t max_bytes;
} nitrfs_fs_t;

// Filesystem API
void   nitrfs_init(nitrfs_fs_t *fs);
void   nitrfs_destroy(nitrfs_fs_t *fs);

int    nitrfs_create(nitrfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm);
int    nitrfs_resize(nitrfs_fs_t *fs, int handle, uint32_t new_capacity);
int    nitrfs_write(nitrfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len);
int    nitrfs_read(nitrfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len);
int    nitrfs_delete(nitrfs_fs_t *fs, int handle);
int    nitrfs_rename(nitrfs_fs_t *fs, int handle, const char *new_name);
int    nitrfs_set_owner(nitrfs_fs_t *fs, int handle, uint32_t owner);

int    nitrfs_get_timestamps(nitrfs_fs_t *fs, int handle, time_t *created, time_t *modified);

int    nitrfs_acl_add(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nitrfs_acl_remove(nitrfs_fs_t *fs, int handle, uint32_t uid);
int    nitrfs_acl_check(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nitrfs_acl_list(nitrfs_fs_t *fs, int handle, nitrfs_acl_entry_t *out, size_t *count);

void   nitrfs_journal_init(void);
void   nitrfs_journal_recover(nitrfs_fs_t *fs);
int    nitrfs_compute_crc(nitrfs_fs_t *fs, int handle);
int    nitrfs_verify(nitrfs_fs_t *fs, int handle);
int    nitrfs_journal_undo_last(nitrfs_fs_t *fs);
int    nitrfs_fsck(nitrfs_fs_t *fs);

void   nitrfs_set_quota(nitrfs_fs_t *fs, uint32_t max_files, uint32_t max_bytes);
void   nitrfs_get_usage(nitrfs_fs_t *fs, uint32_t *used_files, uint32_t *used_bytes);

void   nitrfs_flush_async(nitrfs_fs_t *fs);
void   nitrfs_flush_sync(nitrfs_fs_t *fs);

size_t nitrfs_list(nitrfs_fs_t *fs, char names[][NITRFS_NAME_LEN], size_t max);

int    nitrfs_save_blocks(nitrfs_fs_t *fs, uint8_t *blocks, size_t max_blocks);
int    nitrfs_load_blocks(nitrfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt);
int    nitrfs_save_device(nitrfs_fs_t *fs, uint32_t start_lba);
int    nitrfs_load_device(nitrfs_fs_t *fs, uint32_t start_lba);

extern int block_read(uint32_t lba, uint8_t *buf, size_t count);
extern int block_write(uint32_t lba, const uint8_t *buf, size_t count);

#endif // NITRFS_H

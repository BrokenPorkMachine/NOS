#ifndef NITRFS_H
#define NITRFS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// Cross-platform thread support: use real pthreads if available
#if __has_include(<pthread.h>)
#include <pthread.h>
#else
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)m; (void)a; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
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
} nitrfs_fs_t;

// ====== Filesystem Lifecycle ======
void   nitrfs_init(nitrfs_fs_t *fs);
void   nitrfs_destroy(nitrfs_fs_t *fs);

// ====== File Operations ======
int    nitrfs_create(nitrfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm);
int    nitrfs_resize(nitrfs_fs_t *fs, int handle, uint32_t new_capacity);
int    nitrfs_write(nitrfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len);
int    nitrfs_read(nitrfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len);
int    nitrfs_delete(nitrfs_fs_t *fs, int handle);
int    nitrfs_rename(nitrfs_fs_t *fs, int handle, const char *new_name);
int    nitrfs_set_owner(nitrfs_fs_t *fs, int handle, uint32_t owner);

// ====== Timestamps ======
int    nitrfs_get_timestamps(nitrfs_fs_t *fs, int handle, time_t *created, time_t *modified);

// ====== ACL ======
int    nitrfs_acl_add(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nitrfs_acl_remove(nitrfs_fs_t *fs, int handle, uint32_t uid);
int    nitrfs_acl_check(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nitrfs_acl_list(nitrfs_fs_t *fs, int handle, nitrfs_acl_entry_t *out, size_t *count);

// ====== Integrity & Journaling ======
void   nitrfs_journal_init(void);
void   nitrfs_journal_recover(nitrfs_fs_t *fs);
int    nitrfs_compute_crc(nitrfs_fs_t *fs, int handle);
int    nitrfs_verify(nitrfs_fs_t *fs, int handle);

// ====== Journaling of Deletes/Renames (Undo/Recovery) ======
int    nitrfs_journal_undo_last(nitrfs_fs_t *fs);

// ====== File Listing ======
size_t nitrfs_list(nitrfs_fs_t *fs, char names[][NITRFS_NAME_LEN], size_t max);

// ====== Disk Image/Device Ops ======
int    nitrfs_save_blocks(nitrfs_fs_t *fs, uint8_t *blocks, size_t max_blocks);
int    nitrfs_load_blocks(nitrfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt);
int    nitrfs_save_device(nitrfs_fs_t *fs, uint32_t start_lba);
int    nitrfs_load_device(nitrfs_fs_t *fs, uint32_t start_lba);

// ====== External Block Driver (must be implemented elsewhere) ======
extern int block_read(uint32_t lba, uint8_t *buf, size_t count);
extern int block_write(uint32_t lba, const uint8_t *buf, size_t count);

// Schedule a flush of dirty files to disk/device in background.
// Returns immediately; does not block for IO.
void   nitrfs_flush_async(nitrfs_fs_t *fs);

// Optionally, force a synchronous flush (blocking).
void   nitrfs_flush_sync(nitrfs_fs_t *fs);

// Checks filesystem integrity and repairs if possible. Returns number of errors fixed.
int nitrfs_fsck(nitrfs_fs_t *fs);

// Set a global file or size quota (per-FS, per-user, per-group possible)
void   nitrfs_set_quota(nitrfs_fs_t *fs, uint32_t max_files, uint32_t max_bytes);
// Query the current usage
void   nitrfs_get_usage(nitrfs_fs_t *fs, uint32_t *used_files, uint32_t *used_bytes);

// Force write a journal checkpoint (e.g. after N file ops)
void nitrfs_journal_checkpoint(nitrfs_fs_t *fs);

#endif // NITRFS_H

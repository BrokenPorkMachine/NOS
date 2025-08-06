#ifndef NOSFS_H
#define NOSFS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "../user/libc/libc.h"


#define NOSFS_MAX_FILES    16
#define NOSFS_NAME_LEN     32
#define NOSFS_BLOCK_SIZE   512
#define NOSFS_ACL_MAX      8
#define NOSFS_MAGIC        0x4E495452  // 'NITR'
#define NOSFS_PERM_READ    0x1
#define NOSFS_PERM_WRITE   0x2

/*
 * On‑disk superblock header for NOSFS.
 * - magic: identifies NOSFS volumes
 * - version_{major,minor}: on‑disk format version
 * - journal_lba: starting block of the journal for crash recovery
 * - manifest_lba: block containing embedded NOSM manifest describing
 *                 capabilities, features, and ABI of this filesystem
 * The header allows fast detection of compatible versions and location
 * of the manifest for verifiable, hot‑swappable deployments.
 */
typedef struct {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t journal_lba;
    uint32_t manifest_lba;
} nosfs_superblock_t;

typedef struct {
    uint32_t uid;
    uint32_t perm;
} nosfs_acl_entry_t;

typedef struct {
    char     name[NOSFS_NAME_LEN];
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
    uint32_t perm;
    uint32_t owner;
    uint32_t crc32;
    size_t   acl_count;
    nosfs_acl_entry_t acl[NOSFS_ACL_MAX];
    time_t   created_at;
    time_t   modified_at;
} nosfs_file_t;

typedef struct {
    nosfs_file_t files[NOSFS_MAX_FILES];
    size_t file_count;
    pthread_mutex_t mutex;
    uint32_t max_files;
    uint32_t max_bytes;
} nosfs_fs_t;

// Filesystem API
void   nosfs_init(nosfs_fs_t *fs);
void   nosfs_destroy(nosfs_fs_t *fs);

int    nosfs_create(nosfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm);
int    nosfs_resize(nosfs_fs_t *fs, int handle, uint32_t new_capacity);
int    nosfs_write(nosfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len);
int    nosfs_read(nosfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len);
int    nosfs_delete(nosfs_fs_t *fs, int handle);
int    nosfs_rename(nosfs_fs_t *fs, int handle, const char *new_name);
int    nosfs_set_owner(nosfs_fs_t *fs, int handle, uint32_t owner);

int    nosfs_get_timestamps(nosfs_fs_t *fs, int handle, time_t *created, time_t *modified);

int    nosfs_acl_add(nosfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nosfs_acl_remove(nosfs_fs_t *fs, int handle, uint32_t uid);
int    nosfs_acl_check(nosfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm);
int    nosfs_acl_list(nosfs_fs_t *fs, int handle, nosfs_acl_entry_t *out, size_t *count);

void   nosfs_journal_init(void);
void   nosfs_journal_recover(nosfs_fs_t *fs);
int    nosfs_compute_crc(nosfs_fs_t *fs, int handle);
int    nosfs_verify(nosfs_fs_t *fs, int handle);
int    nosfs_journal_undo_last(nosfs_fs_t *fs);
int    nosfs_fsck(nosfs_fs_t *fs);

void   nosfs_set_quota(nosfs_fs_t *fs, uint32_t max_files, uint32_t max_bytes);
void   nosfs_get_usage(nosfs_fs_t *fs, uint32_t *used_files, uint32_t *used_bytes);

void   nosfs_flush_async(nosfs_fs_t *fs);
void   nosfs_flush_sync(nosfs_fs_t *fs);

size_t nosfs_list(nosfs_fs_t *fs, char names[][NOSFS_NAME_LEN], size_t max);

int    nosfs_save_blocks(nosfs_fs_t *fs, uint8_t *blocks, size_t max_blocks);
int    nosfs_load_blocks(nosfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt);
int    nosfs_save_device(nosfs_fs_t *fs, uint32_t start_lba);
int    nosfs_load_device(nosfs_fs_t *fs, uint32_t start_lba);

extern int block_read(uint32_t lba, uint8_t *buf, size_t count);
extern int block_write(uint32_t lba, const uint8_t *buf, size_t count);

#endif // NOSFS_H

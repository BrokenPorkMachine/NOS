#ifndef NITRFS_H
#define NITRFS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @def NITRFS_MAX_FILES
 *  Maximum files per in-memory filesystem.
 */
#define NITRFS_MAX_FILES   16

/**
 * @def NITRFS_NAME_LEN
 *  File name length (including trailing '\0').
 */
#define NITRFS_NAME_LEN    32

/**
 * @def NITRFS_BLOCK_SIZE
 *  Disk/block export size (in bytes).
 */
#define NITRFS_BLOCK_SIZE  512

/**
 * @def NITRFS_MAGIC
 *  Magic value for disk images.
 */
#define NITRFS_MAGIC       0x4652544E   /* 'NTRF' */

/** File permissions */
#define NITRFS_PERM_READ   0x1
#define NITRFS_PERM_WRITE  0x2

/** In-memory file descriptor */
typedef struct {
    char     name[NITRFS_NAME_LEN];
    uint8_t *data;         /* Allocated buffer (owned) */
    uint32_t size;         /* Bytes used */
    uint32_t capacity;     /* Bytes allocated */
    uint32_t perm;         /* Permission flags */
    uint32_t crc32;        /* Data CRC32 */
} nitrfs_file_t;

/** Filesystem root struct */
typedef struct {
    nitrfs_file_t files[NITRFS_MAX_FILES];
    size_t file_count;
} nitrfs_fs_t;

/* -- Basic operations -- */

/**
 * @brief Initialize a filesystem.
 */
void    nitrfs_init(nitrfs_fs_t *fs);

/**
 * @brief Create a file.
 * @param fs         Filesystem pointer.
 * @param name       Filename (will be truncated).
 * @param capacity   Data buffer size (bytes).
 * @param perm       NITRFS_PERM_READ/WRITE mask.
 * @return file handle (index) or -1 on error.
 */
int     nitrfs_create(nitrfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm);

/**
 * @brief Write data to a file.
 * @return 0 on success, -1 on error.
 */
int     nitrfs_write(nitrfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len);

/**
 * @brief Read data from a file.
 * @return 0 on success, -1 on error.
 */
int     nitrfs_read(nitrfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len);

/**
 * @brief Compute and update CRC32 for a file.
 * @return 0 on success, -1 on error.
 */
int     nitrfs_compute_crc(nitrfs_fs_t *fs, int handle);

/**
 * @brief Verify file data CRC32.
 * @return 0 on match, -1 on mismatch/error.
 */
int     nitrfs_verify(nitrfs_fs_t *fs, int handle);

/**
 * @brief Delete a file.
 * @return 0 on success, -1 on error.
 */
int     nitrfs_delete(nitrfs_fs_t *fs, int handle);

/**
 * @brief Rename a file.
 * @param handle File handle to rename.
 * @param new_name New name string (truncated to NITRFS_NAME_LEN-1).
 * @return 0 on success, -1 on error.
 */
int     nitrfs_rename(nitrfs_fs_t *fs, int handle, const char *new_name);

/**
 * @brief List file names in the FS.
 * @param names  Output buffer [max][NITRFS_NAME_LEN]
 * @param max    Max names to fill
 * @return Count actually filled
 */
size_t  nitrfs_list(nitrfs_fs_t *fs, char names[][NITRFS_NAME_LEN], size_t max);

/* -- Block image import/export -- */

/**
 * @brief Serialize filesystem into blocks.
 * @param blocks     Destination buffer
 * @param max_blocks Number of 512-byte blocks available
 * @return Number of blocks used, or -1 on error.
 */
int     nitrfs_save_blocks(nitrfs_fs_t *fs, uint8_t *blocks, size_t max_blocks);

/**
 * @brief Load a filesystem from a block buffer.
 * @param blocks     Source buffer
 * @param blocks_cnt Number of 512-byte blocks
 * @return 0 on success, -1 on error
 */
int     nitrfs_load_blocks(nitrfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt);

/* -- Disk-backed helpers -- */
int     nitrfs_save_device(nitrfs_fs_t *fs, uint32_t start_lba);
int     nitrfs_load_device(nitrfs_fs_t *fs, uint32_t start_lba);

/* -- Journaling -- */
void    nitrfs_journal_init(void);
void    nitrfs_journal_log(nitrfs_fs_t *fs, int handle);
void    nitrfs_journal_recover(nitrfs_fs_t *fs);

#endif /* NITRFS_H */

#include "nosfs.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define NOSFS_LOG(fmt, ...) /* printf(fmt, ##__VA_ARGS__) */

#define VALID_HANDLE(h, fs) ((h) >= 0 && (h) < NOSFS_MAX_FILES && (fs)->used[h])
#define RETURN_ERR_UNLOCK(mtx) do { nosfs_unlock(mtx); return NOSFS_ERR; } while(0)

// --- Internal helpers ---

static void nosfs_lock(nosfs_fs_t *fs)   { /* pthread_mutex_lock or agent mutex */ }
static void nosfs_unlock(nosfs_fs_t *fs) { /* pthread_mutex_unlock or agent mutex */ }

// --- Initialization ---

int nosfs_init(nosfs_fs_t *fs) {
    memset(fs, 0, sizeof(*fs));
    // Optionally: Load from journal, check quota, validate manifest.
    return 0;
}

// --- File Operations ---

int nosfs_create(nosfs_fs_t *fs, const char *name, int mode, int quota) {
    nosfs_lock(fs);
    for (int i = 0; i < NOSFS_MAX_FILES; ++i) {
        if (!fs->used[i]) {
            memset(&fs->files[i], 0, sizeof(fs->files[i]));
            strncpy(fs->files[i].name, name, NOSFS_NAME_LEN-1);
            fs->files[i].name[NOSFS_NAME_LEN-1] = 0;
            fs->files[i].mode = mode;
            fs->files[i].quota = quota;
            fs->used[i] = 1;
            NOSFS_LOG("File created: %s (handle %d)\n", name, i);
            nosfs_unlock(fs);
            return i;
        }
    }
    NOSFS_LOG("File create failed: no slots\n");
    RETURN_ERR_UNLOCK(fs);
}

int nosfs_write(nosfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    if (!VALID_HANDLE(handle, fs) || !buf) return NOSFS_ERR;
    nosfs_lock(fs);
    nosfs_file_t *f = &fs->files[handle];
    if (offset + len > NOSFS_FILE_SIZE || len > NOSFS_FILE_SIZE) {
        NOSFS_LOG("Write out of bounds: %d+%u>%d\n", offset, len, NOSFS_FILE_SIZE);
        RETURN_ERR_UNLOCK(fs);
    }
    memcpy(f->data + offset, buf, len);
    if (offset + len > f->size) f->size = offset + len;
    // Update CRC if used
    f->crc32 = nosfs_crc32(f->data, f->size);
    NOSFS_LOG("Write %u bytes at %u to handle %d\n", len, offset, handle);
    nosfs_unlock(fs);
    return 0;
}

int nosfs_read(nosfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len) {
    if (!VALID_HANDLE(handle, fs) || !buf) return NOSFS_ERR;
    nosfs_lock(fs);
    nosfs_file_t *f = &fs->files[handle];
    if (offset >= f->size) {
        nosfs_unlock(fs);
        return 0;
    }
    uint32_t to_read = (offset + len > f->size) ? (f->size - offset) : len;
    memcpy(buf, f->data + offset, to_read);
    NOSFS_LOG("Read %u bytes at %u from handle %d\n", to_read, offset, handle);
    nosfs_unlock(fs);
    return 0;
}

int nosfs_delete(nosfs_fs_t *fs, int handle) {
    if (!VALID_HANDLE(handle, fs)) return NOSFS_ERR;
    nosfs_lock(fs);
    memset(&fs->files[handle], 0, sizeof(fs->files[handle]));
    fs->used[handle] = 0;
    NOSFS_LOG("Deleted handle %d\n", handle);
    nosfs_unlock(fs);
    return 0;
}

int nosfs_rename(nosfs_fs_t *fs, int handle, const char *new_name) {
    if (!VALID_HANDLE(handle, fs) || !new_name) return NOSFS_ERR;
    nosfs_lock(fs);
    strncpy(fs->files[handle].name, new_name, NOSFS_NAME_LEN-1);
    fs->files[handle].name[NOSFS_NAME_LEN-1] = 0;
    NOSFS_LOG("Renamed handle %d to %s\n", handle, new_name);
    nosfs_unlock(fs);
    return 0;
}

int nosfs_list(nosfs_fs_t *fs, char (*out)[NOSFS_NAME_LEN], int max) {
    nosfs_lock(fs);
    int count = 0;
    for (int i = 0; i < NOSFS_MAX_FILES && count < max; ++i)
        if (fs->used[i])
            strncpy(out[count++], fs->files[i].name, NOSFS_NAME_LEN);
    nosfs_unlock(fs);
    return count;
}

// --- CRC/Verify/Journaling/Quotas ---

int nosfs_compute_crc(nosfs_fs_t *fs, int handle) {
    if (!VALID_HANDLE(handle, fs)) return NOSFS_ERR;
    nosfs_lock(fs);
    fs->files[handle].crc32 = nosfs_crc32(fs->files[handle].data, fs->files[handle].size);
    nosfs_unlock(fs);
    return 0;
}

int nosfs_verify(nosfs_fs_t *fs, int handle) {
    if (!VALID_HANDLE(handle, fs)) return NOSFS_ERR;
    nosfs_lock(fs);
    uint32_t crc = nosfs_crc32(fs->files[handle].data, fs->files[handle].size);
    nosfs_unlock(fs);
    return (crc == fs->files[handle].crc32) ? 0 : NOSFS_ERR;
}

// TODO: Implement journaling, quotas, and snapshots for multi-file atomicity/security.


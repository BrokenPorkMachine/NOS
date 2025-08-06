#include "nitrfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Minimal libc replacements so the kernel build does not depend on external
// libraries.  These provide deterministic time and realloc implementations.
static time_t nitrfs_time(time_t *t) {
    static time_t current = 0;
    if (t)
        *t = current;
    return current++;
}

static void *nitrfs_realloc(void *ptr, size_t size) {
    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, size);
        free(ptr);
    }
    return new_ptr;
}

#define time    nitrfs_time
#define realloc nitrfs_realloc

// ---------- Journal for normal operations ----------
#define NITRFS_JOURNAL_MAX 32
typedef struct {
    int      handle;
    uint32_t crc32;
} journal_entry_t;
static journal_entry_t journal[NITRFS_JOURNAL_MAX];
static size_t journal_count = 0;

// ---------- Journal for undo (delete/rename) ----------
typedef enum { NITRFS_UNDO_NONE=0, NITRFS_UNDO_DELETE=1, NITRFS_UNDO_RENAME=2 } nitrfs_undo_type_t;
typedef struct {
    nitrfs_undo_type_t type;
    nitrfs_file_t      file_copy;
    int                handle;
    char               old_name[NITRFS_NAME_LEN];
} undo_entry_t;
static undo_entry_t undo_log;

// ========== CRC32 ==========
static uint32_t crc32_compute(const uint8_t *data, uint32_t len) {
    uint32_t crc = ~0u;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
    }
    return ~crc;
}

// ========== Lifecycle ==========
void nitrfs_init(nitrfs_fs_t *fs) {
    memset(fs, 0, sizeof(*fs));
    pthread_mutex_init(&fs->mutex, NULL);
    nitrfs_journal_init();
    undo_log.type = NITRFS_UNDO_NONE;
}

void nitrfs_destroy(nitrfs_fs_t *fs) {
    if (!fs) return;
    pthread_mutex_lock(&fs->mutex);
    for (size_t i = 0; i < fs->file_count; ++i) {
        free(fs->files[i].data);
        fs->files[i].data = NULL;
    }
    fs->file_count = 0;
    pthread_mutex_unlock(&fs->mutex);
    pthread_mutex_destroy(&fs->mutex);
    nitrfs_journal_init();
    undo_log.type = NITRFS_UNDO_NONE;
}

// ========== Helper: Name Valid ==========
static int nitrfs_name_valid(const char *name) {
    return (name && strlen(name) < NITRFS_NAME_LEN);
}

// ========== File Operations ==========

int nitrfs_create(nitrfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm) {
    if (!fs || !nitrfs_name_valid(name) || capacity == 0)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    if (fs->file_count >= NITRFS_MAX_FILES) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        if (strncmp(fs->files[i].name, name, NITRFS_NAME_LEN) == 0) {
            pthread_mutex_unlock(&fs->mutex); return -1;
        }
    }
    nitrfs_file_t *f = &fs->files[fs->file_count];
    strncpy(f->name, name, NITRFS_NAME_LEN-1);
    f->name[NITRFS_NAME_LEN-1] = '\0';
    f->data = calloc(1, capacity);
    if (!f->data) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    f->size = 0;
    f->capacity = capacity;
    f->perm = perm;
    f->crc32 = 0;
    f->owner = 0;
    f->acl_count = 0;
    f->created_at = f->modified_at = time(NULL);
    int h = fs->file_count++;
    pthread_mutex_unlock(&fs->mutex);
    return h;
}

int nitrfs_resize(nitrfs_fs_t *fs, int handle, uint32_t new_capacity) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || new_capacity == 0)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (new_capacity == f->capacity) { pthread_mutex_unlock(&fs->mutex); return 0; }
    uint8_t *newdata = realloc(f->data, new_capacity);
    if (!newdata) { pthread_mutex_unlock(&fs->mutex); return -1; }
    if (new_capacity > f->capacity)
        memset(newdata + f->capacity, 0, new_capacity - f->capacity);
    f->data = newdata;
    f->capacity = new_capacity;
    if (f->size > new_capacity)
        f->size = new_capacity;
    f->modified_at = time(NULL);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_write(nitrfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_WRITE) || offset + len > f->capacity) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    // Journal previous state before write
    for (size_t i = 0; i < journal_count; ++i) {
        if (journal[i].handle == handle) {
            journal[i].crc32 = f->crc32;
            goto skip_add;
        }
    }
    if (journal_count < NITRFS_JOURNAL_MAX) {
        journal[journal_count].handle = handle;
        journal[journal_count].crc32  = f->crc32;
        journal_count++;
    }
skip_add:
    memcpy(f->data + offset, buf, len);
    if (offset + len > f->size)
        f->size = offset + len;
    f->modified_at = time(NULL);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_read(nitrfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_READ) || offset + len > f->size) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    memcpy(buf, f->data + offset, len);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_delete(nitrfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_WRITE)) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    // Journal for undo
    memcpy(&undo_log.file_copy, f, sizeof(nitrfs_file_t));
    undo_log.handle = handle;
    undo_log.type = NITRFS_UNDO_DELETE;
    // Remove
    free(f->data);
    for (size_t i = handle; i + 1 < fs->file_count; ++i)
        fs->files[i] = fs->files[i + 1];
    fs->file_count--;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_journal_undo_last(nitrfs_fs_t *fs) {
    if (!fs || undo_log.type == NITRFS_UNDO_NONE)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    if (undo_log.type == NITRFS_UNDO_DELETE && fs->file_count < NITRFS_MAX_FILES) {
        int handle = fs->file_count++;
        memcpy(&fs->files[handle], &undo_log.file_copy, sizeof(nitrfs_file_t));
        undo_log.type = NITRFS_UNDO_NONE;
        pthread_mutex_unlock(&fs->mutex);
        return handle;
    }
    if (undo_log.type == NITRFS_UNDO_RENAME) {
        int h = undo_log.handle;
        strncpy(fs->files[h].name, undo_log.old_name, NITRFS_NAME_LEN-1);
        fs->files[h].name[NITRFS_NAME_LEN-1] = '\0';
        fs->files[h].modified_at = time(NULL);
        undo_log.type = NITRFS_UNDO_NONE;
        pthread_mutex_unlock(&fs->mutex);
        return h;
    }
    pthread_mutex_unlock(&fs->mutex);
    return -1;
}

int nitrfs_rename(nitrfs_fs_t *fs, int handle, const char *new_name) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !nitrfs_name_valid(new_name))
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_WRITE)) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    // Journal for undo
    strncpy(undo_log.old_name, f->name, NITRFS_NAME_LEN-1);
    undo_log.old_name[NITRFS_NAME_LEN-1] = '\0';
    undo_log.type = NITRFS_UNDO_RENAME;
    undo_log.handle = handle;
    strncpy(f->name, new_name, NITRFS_NAME_LEN-1);
    f->name[NITRFS_NAME_LEN-1] = '\0';
    f->modified_at = time(NULL);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_set_owner(nitrfs_fs_t *fs, int handle, uint32_t owner) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    fs->files[handle].owner = owner;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

// ========== Timestamps ==========
int nitrfs_get_timestamps(nitrfs_fs_t *fs, int handle, time_t *created, time_t *modified) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count) return -1;
    pthread_mutex_lock(&fs->mutex);
    if (created)  *created  = fs->files[handle].created_at;
    if (modified) *modified = fs->files[handle].modified_at;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

// ========== ACL ==========
int nitrfs_acl_add(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    for (size_t i = 0; i < f->acl_count; ++i) {
        if (f->acl[i].uid == uid) { f->acl[i].perm = perm; pthread_mutex_unlock(&fs->mutex); return 0; }
    }
    if (f->acl_count >= NITRFS_ACL_MAX) { pthread_mutex_unlock(&fs->mutex); return -1; }
    f->acl[f->acl_count].uid = uid; f->acl[f->acl_count].perm = perm; f->acl_count++;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_acl_remove(nitrfs_fs_t *fs, int handle, uint32_t uid) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    for (size_t i = 0; i < f->acl_count; ++i) {
        if (f->acl[i].uid == uid) {
            for (size_t j = i; j + 1 < f->acl_count; ++j)
                f->acl[j] = f->acl[j+1];
            f->acl_count--;
            pthread_mutex_unlock(&fs->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&fs->mutex);
    return -1;
}

int nitrfs_acl_check(nitrfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count) return 0;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    if (uid == f->owner && (f->perm & perm) == perm) { pthread_mutex_unlock(&fs->mutex); return 1; }
    for (size_t i = 0; i < f->acl_count; ++i)
        if (f->acl[i].uid == uid && (f->acl[i].perm & perm) == perm) {
            pthread_mutex_unlock(&fs->mutex); return 1;
        }
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nitrfs_acl_list(nitrfs_fs_t *fs, int handle, nitrfs_acl_entry_t *out, size_t *count) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !out || !count) return -1;
    pthread_mutex_lock(&fs->mutex);
    nitrfs_file_t *f = &fs->files[handle];
    size_t n = *count < f->acl_count ? *count : f->acl_count;
    memcpy(out, f->acl, n * sizeof(nitrfs_acl_entry_t));
    *count = n;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

// ========== Journaling ==========

void nitrfs_journal_init(void) { journal_count = 0; }
void nitrfs_journal_recover(nitrfs_fs_t *fs) {
    pthread_mutex_lock(&fs->mutex);
    for (size_t i = 0; i < journal_count; ++i) {
        int h = journal[i].handle;
        if (h < 0 || (size_t)h >= fs->file_count) continue;
        nitrfs_file_t *f = &fs->files[h];
        uint32_t current = crc32_compute(f->data, f->size);
        if (current != journal[i].crc32) {
            memset(f->data, 0, f->capacity);
            f->size  = 0;
            f->crc32 = 0;
        }
    }
    journal_count = 0;
    pthread_mutex_unlock(&fs->mutex);
}
int nitrfs_compute_crc(nitrfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count) return -1;
    pthread_mutex_lock(&fs->mutex);
    fs->files[handle].crc32 = crc32_compute(fs->files[handle].data, fs->files[handle].size);
    for (size_t i = 0; i < journal_count; ++i)
        if (journal[i].handle == handle) { // Clear journal entry
            for (size_t j = i; j+1 < journal_count; ++j)
                journal[j] = journal[j+1];
            journal_count--; break;
        }
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}
int nitrfs_verify(nitrfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count) return -1;
    pthread_mutex_lock(&fs->mutex);
    int res = (fs->files[handle].crc32 == crc32_compute(fs->files[handle].data, fs->files[handle].size)) ? 0 : -1;
    pthread_mutex_unlock(&fs->mutex);
    return res;
}

// ========== File List ==========
size_t nitrfs_list(nitrfs_fs_t *fs, char names[][NITRFS_NAME_LEN], size_t max) {
    if (!fs) return 0;
    pthread_mutex_lock(&fs->mutex);
    size_t count = fs->file_count < max ? fs->file_count : max;
    for (size_t i = 0; i < count; ++i) {
        strncpy(names[i], fs->files[i].name, NITRFS_NAME_LEN-1);
        names[i][NITRFS_NAME_LEN-1] = '\0';
    }
    pthread_mutex_unlock(&fs->mutex);
    return count;
}

// ========== Disk/Device IO ==========
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t file_count;
} disk_header_t;

typedef struct __attribute__((packed)) {
    char     name[NITRFS_NAME_LEN];
    uint32_t size;
    uint32_t perm;
    uint32_t crc32;
    uint32_t owner;
    time_t   created_at;
    time_t   modified_at;
} disk_entry_t;

int nitrfs_save_blocks(nitrfs_fs_t *fs, uint8_t *blocks, size_t max_blocks) {
    if (!fs || !blocks) return -1;
    pthread_mutex_lock(&fs->mutex);
    disk_header_t hdr = { NITRFS_MAGIC, 1, fs->file_count };
    size_t bytes = sizeof(hdr) + fs->file_count * sizeof(disk_entry_t);
    for (size_t i = 0; i < fs->file_count; ++i)
        bytes += fs->files[i].size;
    size_t need_blocks = (bytes + NITRFS_BLOCK_SIZE - 1) / NITRFS_BLOCK_SIZE;
    if (need_blocks > max_blocks) { pthread_mutex_unlock(&fs->mutex); return -1; }

    uint8_t *p = blocks;
    memcpy(p, &hdr, sizeof(hdr)); p += sizeof(hdr);
    for (size_t i = 0; i < fs->file_count; ++i) {
        disk_entry_t e;
        strncpy(e.name, fs->files[i].name, NITRFS_NAME_LEN-1);
        e.name[NITRFS_NAME_LEN-1] = '\0';
        e.size  = fs->files[i].size;
        e.perm  = fs->files[i].perm;
        e.crc32 = fs->files[i].crc32;
        e.owner = fs->files[i].owner;
        e.created_at = fs->files[i].created_at;
        e.modified_at = fs->files[i].modified_at;
        memcpy(p, &e, sizeof(e)); p += sizeof(e);
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        memcpy(p, fs->files[i].data, fs->files[i].size);
        p += fs->files[i].size;
    }
    pthread_mutex_unlock(&fs->mutex);
    return need_blocks;
}

int nitrfs_load_blocks(nitrfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt) {
    if (!fs || !blocks) return -1;
    nitrfs_init(fs);
    pthread_mutex_lock(&fs->mutex);
    const uint8_t *p = blocks;
    size_t bytes = blocks_cnt * NITRFS_BLOCK_SIZE;
    if (bytes < sizeof(disk_header_t)) { pthread_mutex_unlock(&fs->mutex); return -1; }
    disk_header_t hdr;
    memcpy(&hdr, p, sizeof(hdr));
    if (hdr.magic != NITRFS_MAGIC || hdr.file_count > NITRFS_MAX_FILES) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    p += sizeof(hdr);
    if (bytes < sizeof(hdr) + hdr.file_count * sizeof(disk_entry_t)) {
        pthread_mutex_unlock(&fs->mutex); return -1;
    }
    for (size_t i = 0; i < hdr.file_count; ++i) {
        disk_entry_t e;
        memcpy(&e, p, sizeof(e)); p += sizeof(e);
        int h = nitrfs_create(fs, e.name, e.size, e.perm);
        if (h < 0) { pthread_mutex_unlock(&fs->mutex); return -1; }
        fs->files[h].size = e.size;
        fs->files[h].crc32 = e.crc32;
        fs->files[h].owner = e.owner;
        fs->files[h].created_at = e.created_at;
        fs->files[h].modified_at = e.modified_at;
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        if ((size_t)(p - blocks) + fs->files[i].size > bytes) {
            pthread_mutex_unlock(&fs->mutex); return -1;
        }
        memcpy(fs->files[i].data, p, fs->files[i].size);
        p += fs->files[i].size;
    }
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

// You must provide these somewhere:
extern int block_read(uint32_t lba, uint8_t *buf, size_t count);
extern int block_write(uint32_t lba, const uint8_t *buf, size_t count);

#define NITRFS_DEVICE_MAX_BLOCKS 128

int nitrfs_save_device(nitrfs_fs_t *fs, uint32_t start_lba) {
    uint8_t buf[NITRFS_DEVICE_MAX_BLOCKS * NITRFS_BLOCK_SIZE];
    int blocks = nitrfs_save_blocks(fs, buf, NITRFS_DEVICE_MAX_BLOCKS);
    if (blocks < 0) return -1;
    for (int i = 0; i < blocks; ++i)
        if (block_write(start_lba + i, buf + i * NITRFS_BLOCK_SIZE, 1) != 1)
            return -1;
    return blocks;
}

int nitrfs_load_device(nitrfs_fs_t *fs, uint32_t start_lba) {
    uint8_t buf[NITRFS_DEVICE_MAX_BLOCKS * NITRFS_BLOCK_SIZE];
    for (int i = 0; i < NITRFS_DEVICE_MAX_BLOCKS; ++i)
        if (block_read(start_lba + i, buf + i * NITRFS_BLOCK_SIZE, 1) != 1)
            return -1;
    return nitrfs_load_blocks(fs, buf, NITRFS_DEVICE_MAX_BLOCKS);
}

#include "nosfs.h"
#include "../../../nosm/drivers/IO/block.h"
#include <stdatomic.h>

// Global in-memory filesystem used by both the server thread and
// the fs_read_all() helper for the agent loader.
nosfs_fs_t nosfs_root;
// Simple readiness flag shared with nosfs_server.c so fs_read_all() knows when
// the filesystem has been initialised.  Using an atomic ensures visibility
// across threads without needing extra locks.
_Atomic int nosfs_ready = 0;

// ---------- Quota helpers ----------
void nosfs_set_quota(nosfs_fs_t *fs, uint32_t max_files, uint32_t max_bytes) {
    pthread_mutex_lock(&fs->mutex);
    fs->max_files = max_files;
    fs->max_bytes = max_bytes;
    pthread_mutex_unlock(&fs->mutex);
}

void nosfs_get_usage(nosfs_fs_t *fs, uint32_t *used_files, uint32_t *used_bytes) {
    pthread_mutex_lock(&fs->mutex);
    uint32_t files = fs->file_count, bytes = 0;
    for (size_t i = 0; i < fs->file_count; ++i)
        bytes += fs->files[i].size;
    if (used_files) *used_files = files;
    if (used_bytes) *used_bytes = bytes;
    pthread_mutex_unlock(&fs->mutex);
}

// ---------- FSCK ----------
int nosfs_fsck(nosfs_fs_t *fs) {
    int errors = 0;
    pthread_mutex_lock(&fs->mutex);
    for (size_t i = 0; i < fs->file_count; ++i) {
        nosfs_file_t *f = &fs->files[i];
        if (f->size > f->capacity) {
            f->size = f->capacity;
            errors++;
        }
        if (nosfs_compute_crc(fs, i) != 0 || nosfs_verify(fs, i) != 0)
            errors++;
    }
    pthread_mutex_unlock(&fs->mutex);
    return errors;
}

// ---------- Async and Sync Flush ----------
static void* nosfs_flush_worker(void *arg) {
    nosfs_fs_t *fs = (nosfs_fs_t*)arg;
    pthread_mutex_lock(&fs->mutex);
    nosfs_save_device(fs, 0);
    pthread_mutex_unlock(&fs->mutex);
    return NULL;
}

// Simplified async flush – call worker inline for environments without threads
void nosfs_flush_async(nosfs_fs_t *fs) {
    nosfs_flush_worker(fs);
}

void nosfs_flush_sync(nosfs_fs_t *fs) {
    pthread_mutex_lock(&fs->mutex);
    nosfs_save_device(fs, 0);
    pthread_mutex_unlock(&fs->mutex);
}

// Minimal libc replacements for deterministic time/realloc
static time_t nosfs_time(time_t *t) {
    static time_t current = 0;
    if (t)
        *t = current;
    return current++;
}

static void *nosfs_realloc(void *ptr, size_t size) {
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

#define time    nosfs_time
#define realloc nosfs_realloc

// ---------- Journal for normal operations ----------
#define NOSFS_JOURNAL_MAX 32
typedef struct {
    int      handle;
    uint32_t crc32;
} journal_entry_t;
static journal_entry_t journal[NOSFS_JOURNAL_MAX];
static size_t journal_count = 0;

// ---------- Journal for undo (delete/rename) ----------
typedef enum { NOSFS_UNDO_NONE=0, NOSFS_UNDO_DELETE=1, NOSFS_UNDO_RENAME=2 } nosfs_undo_type_t;
typedef struct {
    nosfs_undo_type_t type;
    nosfs_file_t      file_copy;
    int                handle;
    char               old_name[NOSFS_NAME_LEN];
} undo_entry_t;
static undo_entry_t undo_log;
static uint8_t nosfs_io_buffer[BLOCK_DEVICE_BLOCKS * BLOCK_SIZE];

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
void nosfs_init(nosfs_fs_t *fs) {
    memset(fs, 0, sizeof(*fs));
    pthread_mutex_init(&fs->mutex, NULL);
    nosfs_journal_init();
    undo_log.type = NOSFS_UNDO_NONE;
    atomic_store(&nosfs_ready, 1);
}

void nosfs_destroy(nosfs_fs_t *fs) {
    if (!fs) return;
    pthread_mutex_lock(&fs->mutex);
    for (size_t i = 0; i < fs->file_count; ++i) {
        free(fs->files[i].data);
        fs->files[i].data = NULL;
    }
    fs->file_count = 0;
    pthread_mutex_unlock(&fs->mutex);
    pthread_mutex_destroy(&fs->mutex);
    nosfs_journal_init();
    undo_log.type = NOSFS_UNDO_NONE;
    atomic_store(&nosfs_ready, 0);
}

// ========== Helper: Name Valid ==========
static int nosfs_name_valid(const char *name) {
    return (name && strlen(name) < NOSFS_NAME_LEN);
}

// ========== File Operations ==========

int nosfs_create(nosfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm) {
    if (!fs || !nosfs_name_valid(name) || capacity == 0)
        return -1;
    pthread_mutex_lock(&fs->mutex);

    // Quota checks
    if (fs->max_files && fs->file_count >= fs->max_files) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
    uint32_t total_bytes = 0;
    for (size_t i = 0; i < fs->file_count; ++i)
        total_bytes += fs->files[i].size;
    if (fs->max_bytes && (total_bytes + capacity) > fs->max_bytes) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }

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
    strncpy(f->name, name, NOSFS_NAME_LEN-1);
    f->name[NOSFS_NAME_LEN-1] = '\0';
    f->data = calloc(1, capacity);
    if (!f->data) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
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

int nosfs_resize(nosfs_fs_t *fs, int handle, uint32_t new_capacity) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || new_capacity == 0)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    nosfs_file_t *f = &fs->files[handle];

    // Quota check: do not allow growing past quota
    if (new_capacity > f->capacity && fs->max_bytes) {
        uint32_t total_bytes = 0;
        for (size_t i = 0; i < fs->file_count; ++i)
            total_bytes += fs->files[i].size;
        uint32_t new_total = total_bytes - f->size + new_capacity;
        if (new_total > fs->max_bytes) {
            pthread_mutex_unlock(&fs->mutex);
            return -1;
        }
    }

    if (new_capacity == f->capacity) {
        pthread_mutex_unlock(&fs->mutex);
        return 0;
    }
    uint8_t *newdata = realloc(f->data, new_capacity);
    if (!newdata) {
        pthread_mutex_unlock(&fs->mutex);
        return -1;
    }
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

int nosfs_write(nosfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !buf)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    if (offset + len > f->capacity)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    memcpy(f->data + offset, buf, len);
    if (offset + len > f->size)
        f->size = offset + len;
    f->modified_at = time(NULL);
    int found = 0;
    for (size_t i = 0; i < journal_count; ++i)
        if (journal[i].handle == handle)
            found = 1;
    if (!found && journal_count < NOSFS_JOURNAL_MAX)
        journal[journal_count++].handle = handle;
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nosfs_read(nosfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !buf)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    if (offset + len > f->size)
        return -1;
    memcpy(buf, f->data + offset, len);
    return 0;
}

int nosfs_delete(nosfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    pthread_mutex_lock(&fs->mutex);
    free(fs->files[handle].data);
    fs->files[handle] = fs->files[--fs->file_count];
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nosfs_rename(nosfs_fs_t *fs, int handle, const char *new_name) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !nosfs_name_valid(new_name))
        return -1;
    pthread_mutex_lock(&fs->mutex);
    strncpy(fs->files[handle].name, new_name, NOSFS_NAME_LEN-1);
    fs->files[handle].name[NOSFS_NAME_LEN-1] = '\0';
    fs->files[handle].modified_at = time(NULL);
    pthread_mutex_unlock(&fs->mutex);
    return 0;
}

int nosfs_set_owner(nosfs_fs_t *fs, int handle, uint32_t owner) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    fs->files[handle].owner = owner;
    return 0;
}

int nosfs_get_timestamps(nosfs_fs_t *fs, int handle, time_t *created, time_t *modified) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    if (created) *created = fs->files[handle].created_at;
    if (modified) *modified = fs->files[handle].modified_at;
    return 0;
}

int nosfs_acl_add(nosfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    if (f->acl_count >= NOSFS_ACL_MAX)
        return -1;
    f->acl[f->acl_count++] = (nosfs_acl_entry_t){uid, perm};
    return 0;
}

int nosfs_acl_remove(nosfs_fs_t *fs, int handle, uint32_t uid) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    for (size_t i = 0; i < f->acl_count; ++i) {
        if (f->acl[i].uid == uid) {
            f->acl[i] = f->acl[--f->acl_count];
            return 0;
        }
    }
    return -1;
}

int nosfs_acl_check(nosfs_fs_t *fs, int handle, uint32_t uid, uint32_t perm) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return 0;
    nosfs_file_t *f = &fs->files[handle];
    for (size_t i = 0; i < f->acl_count; ++i)
        if (f->acl[i].uid == uid && (f->acl[i].perm & perm))
            return 1;
    return 0;
}

int nosfs_acl_list(nosfs_fs_t *fs, int handle, nosfs_acl_entry_t *out, size_t *count) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count || !out || !count)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    size_t n = f->acl_count;
    if (*count < n) n = *count;
    memcpy(out, f->acl, n * sizeof(nosfs_acl_entry_t));
    *count = n;
    return 0;
}

void nosfs_journal_init(void) { journal_count = 0; }

void nosfs_journal_recover(nosfs_fs_t *fs) {
    for (size_t i = 0; i < journal_count; ++i) {
        int h = journal[i].handle;
        if ((size_t)h < fs->file_count)
            fs->files[h].size = 0;
    }
    journal_count = 0;
}

int nosfs_compute_crc(nosfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    f->crc32 = crc32_compute(f->data, f->size);
    for (size_t i = 0; i < journal_count; ++i) {
        if (journal[i].handle == handle) {
            journal[i] = journal[--journal_count];
            break;
        }
    }
    return 0;
}

int nosfs_verify(nosfs_fs_t *fs, int handle) {
    if (!fs || handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nosfs_file_t *f = &fs->files[handle];
    return (f->crc32 == crc32_compute(f->data, f->size)) ? 0 : -1;
}

size_t nosfs_list(nosfs_fs_t *fs, char names[][NOSFS_NAME_LEN], size_t max) {
    if (!fs || !names) return 0;
    size_t n = (fs->file_count < max) ? fs->file_count : max;
    for (size_t i = 0; i < n; ++i)
        strncpy(names[i], fs->files[i].name, NOSFS_NAME_LEN);
    return n;
}

int nosfs_save_blocks(nosfs_fs_t *fs, uint8_t *blocks, size_t max_blocks) {
    if (!fs || !blocks) return -1;
    size_t offset = 0;
    memcpy(blocks + offset, &fs->file_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    for (size_t i = 0; i < fs->file_count; ++i) {
        nosfs_file_t *f = &fs->files[i];
        memcpy(blocks + offset, f->name, NOSFS_NAME_LEN);
        offset += NOSFS_NAME_LEN;
        memcpy(blocks + offset, &f->size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(blocks + offset, &f->capacity, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(blocks + offset, f->data, f->size);
        offset += f->size;
    }
    size_t needed_blocks = (offset + NOSFS_BLOCK_SIZE - 1) / NOSFS_BLOCK_SIZE;
    if (needed_blocks > max_blocks) return -1;
    return (int)needed_blocks;
}

int nosfs_load_blocks(nosfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt) {
    (void)blocks_cnt;
    if (!fs || !blocks) return -1;
    size_t offset = 0;
    uint32_t count = 0;
    memcpy(&count, blocks + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    fs->file_count = count;
    for (size_t i = 0; i < count; ++i) {
        nosfs_file_t *f = &fs->files[i];
        memcpy(f->name, blocks + offset, NOSFS_NAME_LEN);
        offset += NOSFS_NAME_LEN;
        memcpy(&f->size, blocks + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(&f->capacity, blocks + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        f->data = malloc(f->capacity);
        memcpy(f->data, blocks + offset, f->size);
        offset += f->size;
        f->perm = NOSFS_PERM_READ | NOSFS_PERM_WRITE;
        f->owner = 0;
        f->crc32 = crc32_compute(f->data, f->size);
        f->acl_count = 0;
        f->created_at = f->modified_at = time(NULL);
    }
    pthread_mutex_init(&fs->mutex, NULL);
    return 0;
}

int nosfs_save_device(nosfs_fs_t *fs, uint32_t start_lba) {
    size_t bytes = sizeof(uint32_t);
    for (size_t i = 0; i < fs->file_count; ++i)
        bytes += NOSFS_NAME_LEN + sizeof(uint32_t) * 2 + fs->files[i].size;
    size_t blocks = (bytes + NOSFS_BLOCK_SIZE - 1) / NOSFS_BLOCK_SIZE;
    if (nosfs_save_blocks(fs, nosfs_io_buffer, blocks) < 0 ||
        block_write(start_lba, nosfs_io_buffer, blocks) < 0)
        return -1;
    return (int)blocks;
}

int nosfs_load_device(nosfs_fs_t *fs, uint32_t start_lba) {
    size_t total_blocks = BLOCK_DEVICE_BLOCKS;
    if (block_read(start_lba, nosfs_io_buffer, total_blocks) < 0)
        return -1;
    nosfs_load_blocks(fs, nosfs_io_buffer, total_blocks);
    return 0;
}

int nosfs_journal_undo_last(nosfs_fs_t *fs) { (void)fs; return -1; }

// Helper for the kernel agent loader: fetch entire file contents
// into a newly allocated buffer.  Accept both "/agents/foo" and "agents/foo".
int fs_read_all(const char *path, void **out, size_t *out_sz) {
    if (!atomic_load(&nosfs_ready) || !path || !out || !out_sz)
        return -1;

    const char *original = path;
    const char *normalized = (*path == '/') ? path + 1 : path;

    pthread_mutex_lock(&nosfs_root.mutex);

    // First try WITHOUT leading slash (canonical in our store)
    for (size_t i = 0; i < nosfs_root.file_count; ++i) {
        nosfs_file_t *f = &nosfs_root.files[i];
        if (strcmp(f->name, normalized) == 0) {
            void *buf = malloc(f->size);
            if (!buf) { pthread_mutex_unlock(&nosfs_root.mutex); return -1; }
            memcpy(buf, f->data, f->size);
            *out = buf; *out_sz = f->size;
            pthread_mutex_unlock(&nosfs_root.mutex);
            return 0;
        }
    }

    // If caller passed a name WITHOUT slash while the store has WITH slash (unlikely but safe):
    if (normalized == original) { // there was no leading slash in input
        for (size_t i = 0; i < nosfs_root.file_count; ++i) {
            nosfs_file_t *f = &nosfs_root.files[i];
            if (f->name[0] == '/' && strcmp(f->name + 1, normalized) == 0) {
                void *buf = malloc(f->size);
                if (!buf) { pthread_mutex_unlock(&nosfs_root.mutex); return -1; }
                memcpy(buf, f->data, f->size);
                *out = buf; *out_sz = f->size;
                pthread_mutex_unlock(&nosfs_root.mutex);
                return 0;
            }
        }
    }

    pthread_mutex_unlock(&nosfs_root.mutex);
    return -1;
}

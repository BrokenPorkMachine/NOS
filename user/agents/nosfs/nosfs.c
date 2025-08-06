#include "nosfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // for usleep if needed

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
    // ... flush dirty buffers/journal to disk/device ...
    pthread_mutex_unlock(&fs->mutex);
    return NULL;
}

void nosfs_flush_async(nosfs_fs_t *fs) {
    pthread_t thread;
    pthread_create(&thread, NULL, nosfs_flush_worker, fs);
    pthread_detach(thread);
}

void nosfs_flush_sync(nosfs_fs_t *fs) {
    pthread_mutex_lock(&fs->mutex);
    // ... flush everything immediately ...
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

// ... rest of your previously posted functions unchanged ...

// [SNIP: The rest of your code continues as previously, unchanged.]
// This includes journaling, ACL, timestamps, device IO, etc.

// --------- End of file ---------

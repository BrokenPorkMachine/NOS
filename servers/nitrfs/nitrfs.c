#include "nitrfs.h"
#include <string.h>
#include <stdlib.h>

static uint32_t crc32_compute(const uint8_t *data, uint32_t len) {
    uint32_t crc = ~0u;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        }
    }
    return ~crc;
}

void nitrfs_init(nitrfs_fs_t *fs) {
    memset(fs, 0, sizeof(*fs));
}

int nitrfs_create(nitrfs_fs_t *fs, const char *name, uint32_t capacity, uint32_t perm) {
    if (fs->file_count >= NITRFS_MAX_FILES)
        return -1;
    nitrfs_file_t *f = &fs->files[fs->file_count];
    strncpy(f->name, name, NITRFS_NAME_LEN-1);
    f->name[NITRFS_NAME_LEN-1] = '\0';
    f->data = calloc(1, capacity);
    if (!f->data)
        return -1;
    f->size = 0;
    f->capacity = capacity;
    f->perm = perm;
    f->crc32 = 0;
    return fs->file_count++;
}

int nitrfs_write(nitrfs_fs_t *fs, int handle, uint32_t offset, const void *buf, uint32_t len) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_WRITE))
        return -1;
    if (offset + len > f->capacity)
        return -1;
    memcpy(f->data + offset, buf, len);
    if (offset + len > f->size)
        f->size = offset + len;
    return 0;
}

int nitrfs_read(nitrfs_fs_t *fs, int handle, uint32_t offset, void *buf, uint32_t len) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    if (!(f->perm & NITRFS_PERM_READ))
        return -1;
    if (offset + len > f->size)
        return -1;
    memcpy(buf, f->data + offset, len);
    return 0;
}

int nitrfs_compute_crc(nitrfs_fs_t *fs, int handle) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    f->crc32 = crc32_compute(f->data, f->size);
    return 0;
}

int nitrfs_verify(nitrfs_fs_t *fs, int handle) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    return f->crc32 == crc32_compute(f->data, f->size) ? 0 : -1;
}

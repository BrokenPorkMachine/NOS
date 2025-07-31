#include "nitrfs.h"
#include "../../src/libc.h"

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

int nitrfs_delete(nitrfs_fs_t *fs, int handle) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    free(f->data);
    for (size_t i = handle; i + 1 < fs->file_count; ++i)
        fs->files[i] = fs->files[i + 1];
    fs->file_count--;
    return 0;
}

int nitrfs_rename(nitrfs_fs_t *fs, int handle, const char *new_name) {
    if (handle < 0 || (size_t)handle >= fs->file_count)
        return -1;
    if (!new_name)
        return -1;
    nitrfs_file_t *f = &fs->files[handle];
    strncpy(f->name, new_name, NITRFS_NAME_LEN - 1);
    f->name[NITRFS_NAME_LEN - 1] = '\0';
    return 0;
}

size_t nitrfs_list(nitrfs_fs_t *fs, char names[][NITRFS_NAME_LEN], size_t max) {
    size_t count = fs->file_count < max ? fs->file_count : max;
    for (size_t i = 0; i < count; ++i)
        strncpy(names[i], fs->files[i].name, NITRFS_NAME_LEN);
    return count;
}

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
} disk_entry_t;

int nitrfs_save_blocks(nitrfs_fs_t *fs, uint8_t *blocks, size_t max_blocks) {
    disk_header_t hdr = { NITRFS_MAGIC, 1, fs->file_count };
    size_t bytes = sizeof(hdr) + fs->file_count * sizeof(disk_entry_t);
    for (size_t i = 0; i < fs->file_count; ++i)
        bytes += fs->files[i].size;
    size_t need_blocks = (bytes + NITRFS_BLOCK_SIZE - 1) / NITRFS_BLOCK_SIZE;
    if (need_blocks > max_blocks)
        return -1;

    uint8_t *p = blocks;
    memcpy(p, &hdr, sizeof(hdr));
    p += sizeof(hdr);
    for (size_t i = 0; i < fs->file_count; ++i) {
        disk_entry_t e;
        strncpy(e.name, fs->files[i].name, NITRFS_NAME_LEN);
        e.size  = fs->files[i].size;
        e.perm  = fs->files[i].perm;
        e.crc32 = fs->files[i].crc32;
        memcpy(p, &e, sizeof(e));
        p += sizeof(e);
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        memcpy(p, fs->files[i].data, fs->files[i].size);
        p += fs->files[i].size;
    }
    return need_blocks;
}

int nitrfs_load_blocks(nitrfs_fs_t *fs, const uint8_t *blocks, size_t blocks_cnt) {
    nitrfs_init(fs);
    const uint8_t *p = blocks;
    size_t bytes = blocks_cnt * NITRFS_BLOCK_SIZE;
    if (bytes < sizeof(disk_header_t))
        return -1;
    disk_header_t hdr;
    memcpy(&hdr, p, sizeof(hdr));
    if (hdr.magic != NITRFS_MAGIC || hdr.file_count > NITRFS_MAX_FILES)
        return -1;
    p += sizeof(hdr);
    if (bytes < sizeof(hdr) + hdr.file_count * sizeof(disk_entry_t))
        return -1;
    for (size_t i = 0; i < hdr.file_count; ++i) {
        disk_entry_t e;
        memcpy(&e, p, sizeof(e));
        p += sizeof(e);
        int h = nitrfs_create(fs, e.name, e.size, e.perm);
        if (h < 0)
            return -1;
        fs->files[h].size = e.size;
        fs->files[h].crc32 = e.crc32;
    }
    for (size_t i = 0; i < fs->file_count; ++i) {
        if ((size_t)(p - blocks) + fs->files[i].size > bytes)
            return -1;
        memcpy(fs->files[i].data, p, fs->files[i].size);
        p += fs->files[i].size;
    }
    return 0;
}

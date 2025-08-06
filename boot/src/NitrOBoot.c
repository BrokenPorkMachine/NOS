#include "nitrfs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Dummy block_read/write for memory testing
int block_read(uint32_t lba, uint8_t *buf, size_t count) {
    (void)lba; (void)buf; (void)count; return 1;
}
int block_write(uint32_t lba, const uint8_t *buf, size_t count) {
    (void)lba; (void)buf; (void)count; return 1;
}

int main(void) {
    nitrfs_fs_t fs;
    nitrfs_init(&fs);

    int h = nitrfs_create(&fs, "hello.txt", 256, NITRFS_PERM_READ|NITRFS_PERM_WRITE);
    assert(h >= 0);

    const char *msg = "NitroFS is cool!";
    assert(0 == nitrfs_write(&fs, h, 0, msg, strlen(msg)));

    // CRC/verify
    nitrfs_compute_crc(&fs, h);
    assert(nitrfs_verify(&fs, h) == 0);

    // ACLs
    assert(nitrfs_acl_add(&fs, h, 123, NITRFS_PERM_READ|NITRFS_PERM_WRITE) == 0);

    // Resize
    assert(nitrfs_resize(&fs, h, 1024) == 0);

    // List files
    char names[16][NITRFS_NAME_LEN];
    size_t n = nitrfs_list(&fs, names, 16);
    for (size_t i = 0; i < n; ++i) printf("File: %s\n", names[i]);

    // Delete and undo
    assert(nitrfs_delete(&fs, h) == 0);
    assert(nitrfs_journal_undo_last(&fs) >= 0);

    nitrfs_destroy(&fs);
    printf("NitroFS test OK\n");
    return 0;
}

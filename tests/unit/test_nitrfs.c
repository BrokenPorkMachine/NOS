#include <assert.h>
#include <string.h>
#include "../../user/servers/nitrfs/nitrfs.h"
#include "../../user/libc/libc.h"
#include "../../kernel/drivers/IO/block.h"

int main(void) {
    block_init();
    nitrfs_fs_t fs;
    nitrfs_init(&fs);
    int h = nitrfs_create(&fs, "file.txt", 16, NITRFS_PERM_READ | NITRFS_PERM_WRITE);
    assert(h >= 0);
    /* duplicate name should fail */
    assert(nitrfs_create(&fs, "file.txt", 16, NITRFS_PERM_READ) == -1);
    const char *data = "hi";
    assert(nitrfs_write(&fs, h, 0, data, 2) == 0);
    char buf[4];
    assert(nitrfs_read(&fs, h, 0, buf, 2) == 0);
    buf[2] = '\0';
    assert(strcmp(buf, "hi") == 0);
    nitrfs_compute_crc(&fs, h);
    assert(nitrfs_verify(&fs, h) == 0);

    /* ACL tests */
    assert(nitrfs_acl_add(&fs, h, 1, NITRFS_PERM_READ) == 0);
    assert(nitrfs_acl_check(&fs, h, 1, NITRFS_PERM_READ) == 1);
    assert(nitrfs_acl_check(&fs, h, 1, NITRFS_PERM_WRITE) == 0);

    /* Save to mock device and reload */
    assert(nitrfs_save_device(&fs, 0) > 0);
    nitrfs_fs_t fs2;
    assert(nitrfs_load_device(&fs2, 0) == 0);
    char buf2[4];
    assert(nitrfs_read(&fs2, 0, 0, buf2, 2) == 0);
    buf2[2] = '\0';
    assert(strcmp(buf2, "hi") == 0);
    assert(nitrfs_verify(&fs2, 0) == 0);

    /* Journaling recovery test */
    int j = nitrfs_create(&fs2, "log.txt", 16, NITRFS_PERM_READ | NITRFS_PERM_WRITE);
    assert(j >= 0);
    assert(nitrfs_write(&fs2, j, 0, "AA", 2) == 0);
    nitrfs_compute_crc(&fs2, j);
    assert(nitrfs_write(&fs2, j, 0, "BB", 2) == 0); /* no commit */
    nitrfs_journal_recover(&fs2);
    assert(fs2.files[j].size == 0);

    return 0;
}

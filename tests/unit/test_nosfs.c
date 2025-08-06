#include <assert.h>
#include <string.h>
#include "../../include/nosfs.h"
#include "../../user/libc/libc.h"
#include "../../kernel/drivers/IO/block.h"

int main(void) {
    block_init();
    nosfs_fs_t fs;
    nosfs_init(&fs);
    int h = nosfs_create(&fs, "file.txt", 16, NOSFS_PERM_READ | NOSFS_PERM_WRITE);
    assert(h >= 0);
    /* duplicate name should fail */
    assert(nosfs_create(&fs, "file.txt", 16, NOSFS_PERM_READ) == -1);
    const char *data = "hi";
    assert(nosfs_write(&fs, h, 0, data, 2) == 0);
    char buf[4];
    assert(nosfs_read(&fs, h, 0, buf, 2) == 0);
    buf[2] = '\0';
    assert(strcmp(buf, "hi") == 0);
    nosfs_compute_crc(&fs, h);
    assert(nosfs_verify(&fs, h) == 0);

    /* Offset write/read */
    assert(nosfs_write(&fs, h, 2, "xy", 2) == 0);
    assert(nosfs_read(&fs, h, 1, buf, 3) == 0);
    buf[3] = '\0';
    assert(strcmp(buf, "ixy") == 0);
    nosfs_compute_crc(&fs, h);
    assert(nosfs_verify(&fs, h) == 0);

    /* ACL tests */
    assert(nosfs_acl_add(&fs, h, 1, NOSFS_PERM_READ) == 0);
    assert(nosfs_acl_check(&fs, h, 1, NOSFS_PERM_READ) == 1);
    assert(nosfs_acl_check(&fs, h, 1, NOSFS_PERM_WRITE) == 0);

    /* Save to mock device and reload */
    assert(nosfs_save_device(&fs, 0) > 0);
    nosfs_fs_t fs2;
    assert(nosfs_load_device(&fs2, 0) == 0);
    char buf2[4];
    assert(nosfs_read(&fs2, 0, 0, buf2, 2) == 0);
    buf2[2] = '\0';
    assert(strcmp(buf2, "hi") == 0);
    assert(nosfs_verify(&fs2, 0) == 0);

    /* Journaling recovery test */
    int j = nosfs_create(&fs2, "log.txt", 16, NOSFS_PERM_READ | NOSFS_PERM_WRITE);
    assert(j >= 0);
    assert(nosfs_write(&fs2, j, 0, "AA", 2) == 0);
    nosfs_compute_crc(&fs2, j);
    assert(nosfs_write(&fs2, j, 0, "BB", 2) == 0); /* no commit */
    nosfs_journal_recover(&fs2);
    assert(fs2.files[j].size == 0);

    return 0;
}

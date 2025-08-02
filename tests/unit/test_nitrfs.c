#include <assert.h>
#include <string.h>
#include "../../user/servers/nitrfs/nitrfs.h"
#include "../../user/libc/libc.h"

int main(void) {
    nitrfs_fs_t fs;
    nitrfs_init(&fs);
    int h = nitrfs_create(&fs, "file.txt", 16, NITRFS_PERM_READ | NITRFS_PERM_WRITE);
    assert(h >= 0);
    const char *data = "hi";
    assert(nitrfs_write(&fs, h, 0, data, 2) == 0);
    char buf[4];
    assert(nitrfs_read(&fs, h, 0, buf, 2) == 0);
    buf[2] = '\0';
    assert(strcmp(buf, "hi") == 0);
    nitrfs_compute_crc(&fs, h);
    assert(nitrfs_verify(&fs, h) == 0);
    assert(nitrfs_rename(&fs, h, "new.txt") == 0);
    assert(nitrfs_delete(&fs, h) == 0);
    return 0;
}

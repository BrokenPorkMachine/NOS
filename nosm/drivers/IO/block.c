#include "block.h"
#include "sata.h"
#include <string.h>

/*
 * Simple pluggable block backend.
 * By default a small RAM disk is used so unit tests and early boot
 * continue to function even when no physical disk is present.  The
 * backend can be switched to the SATA driver by calling
 * block_use_sata(), which attempts to initialise the real device and
 * swaps the read/write function pointers if successful.
 */

static uint8_t storage[BLOCK_DEVICE_BLOCKS * BLOCK_SIZE];

static int ramdisk_read(uint32_t lba, uint8_t *buf, size_t count)
{
    if (lba + count > BLOCK_DEVICE_BLOCKS)
        return -1;
    memcpy(buf, &storage[lba * BLOCK_SIZE], count * BLOCK_SIZE);
    return (int)count;
}

static int ramdisk_write(uint32_t lba, const uint8_t *buf, size_t count)
{
    if (lba + count > BLOCK_DEVICE_BLOCKS)
        return -1;
    memcpy(&storage[lba * BLOCK_SIZE], buf, count * BLOCK_SIZE);
    return (int)count;
}

/* Function pointers to the active backend. */
static int (*read_fn)(uint32_t, uint8_t *, size_t)  = ramdisk_read;
static int (*write_fn)(uint32_t, const uint8_t *, size_t) = ramdisk_write;

/* Weak SATA hooks so tests link even without the real driver. */
__attribute__((weak)) int sata_init(void) { return -1; }
__attribute__((weak)) int sata_read_block(uint32_t lba, uint8_t *buf, size_t cnt)
{ (void)lba; (void)buf; (void)cnt; return -1; }
__attribute__((weak)) int sata_write_block(uint32_t lba, const uint8_t *buf, size_t cnt)
{ (void)lba; (void)buf; (void)cnt; return -1; }

void block_init(void)
{
    memset(storage, 0, sizeof(storage));
    read_fn  = ramdisk_read;
    write_fn = ramdisk_write;
}

int block_use_sata(void)
{
    if (sata_init() == 0) {
        read_fn  = sata_read_block;
        write_fn = sata_write_block;
        return 0;
    }
    return -1;
}

int block_read(uint32_t lba, uint8_t *buf, size_t count)
{
    return read_fn(lba, buf, count);
}

int block_write(uint32_t lba, const uint8_t *buf, size_t count)
{
    return write_fn(lba, buf, count);
}

int block_handle_ipc(ipc_message_t *msg)
{
    if (!msg)
        return -1;
    switch (msg->type) {
    case BLOCK_MSG_READ: {
        uint32_t lba = msg->arg1;
        uint32_t count = msg->arg2;
        if (count * BLOCK_SIZE > IPC_MSG_DATA_MAX)
            count = IPC_MSG_DATA_MAX / BLOCK_SIZE;
        if (block_read(lba, msg->data, count) < 0)
            return -1;
        msg->len = count * BLOCK_SIZE;
        return 0;
    }
    case BLOCK_MSG_WRITE: {
        uint32_t lba = msg->arg1;
        uint32_t count = msg->len / BLOCK_SIZE;
        return block_write(lba, msg->data, count);
    }
    default:
        return -1;
    }
}

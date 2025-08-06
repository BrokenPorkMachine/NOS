#include "block.h"
#include <string.h>

static uint8_t storage[BLOCK_DEVICE_BLOCKS * BLOCK_SIZE];

void block_init(void) {
    memset(storage, 0, sizeof(storage));
}

int block_read(uint32_t lba, uint8_t *buf, size_t count) {
    if (lba + count > BLOCK_DEVICE_BLOCKS)
        return -1;
    memcpy(buf, &storage[lba * BLOCK_SIZE], count * BLOCK_SIZE);
    return (int)count;
}

int block_write(uint32_t lba, const uint8_t *buf, size_t count) {
    if (lba + count > BLOCK_DEVICE_BLOCKS)
        return -1;
    memcpy(&storage[lba * BLOCK_SIZE], buf, count * BLOCK_SIZE);
    return (int)count;
}

int block_handle_ipc(ipc_message_t *msg) {
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

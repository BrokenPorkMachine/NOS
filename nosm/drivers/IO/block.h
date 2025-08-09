#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>
#include "../../kernel/IPC/ipc.h"

#define BLOCK_SIZE 512
#define BLOCK_DEVICE_BLOCKS 2048

void block_init(void);
int  block_read(uint32_t lba, uint8_t *buf, size_t count);
int  block_write(uint32_t lba, const uint8_t *buf, size_t count);

#define BLOCK_MSG_READ  0x2000
#define BLOCK_MSG_WRITE 0x2001

int block_handle_ipc(ipc_message_t *msg);

#endif /* BLOCK_H */

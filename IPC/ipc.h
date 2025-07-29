#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_DATA_MAX 64
#define IPC_QUEUE_SIZE   16

typedef struct {
    uint32_t type;
    uint32_t arg1;
    uint32_t arg2;
    uint8_t  data[IPC_MSG_DATA_MAX];
} ipc_message_t;

typedef struct {
    ipc_message_t msgs[IPC_QUEUE_SIZE];
    size_t head;
    size_t tail;
} ipc_queue_t;

void ipc_init(ipc_queue_t *q);
int  ipc_send(ipc_queue_t *q, const ipc_message_t *msg);
int  ipc_receive(ipc_queue_t *q, ipc_message_t *msg);

#endif // IPC_H

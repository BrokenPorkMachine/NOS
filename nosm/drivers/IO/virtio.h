#ifndef VIRTIO_H
#define VIRTIO_H

#include <stdint.h>

int virtio_blk_init(void);
int virtio_blk_read(uint64_t sector, void *buf);

#endif // VIRTIO_H

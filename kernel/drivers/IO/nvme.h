#ifndef NVME_H
#define NVME_H

#include <stdint.h>
#include <stddef.h>

int nvme_init(void);
int nvme_read_block(uint32_t lba, uint8_t *buf, size_t count);
int nvme_write_block(uint32_t lba, const uint8_t *buf, size_t count);

#endif /* NVME_H */

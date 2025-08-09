#ifndef SATA_H
#define SATA_H

#include <stdint.h>
#include <stddef.h>

int sata_init(void);
int sata_read_block(uint32_t lba, uint8_t *buf, size_t count);
int sata_write_block(uint32_t lba, const uint8_t *buf, size_t count);

#endif /* SATA_H */

#include "nvme.h"
#include "block.h"

int nvme_init(void) {
    /* In a real driver, NVMe controller initialization would occur here. */
    return 0;
}

int nvme_read_block(uint32_t lba, uint8_t *buf, size_t count) {
    return block_read(lba, buf, count);
}

int nvme_write_block(uint32_t lba, const uint8_t *buf, size_t count) {
    return block_write(lba, buf, count);
}

#include "sata.h"
#include "block.h"

int sata_init(void) {
    /* In a real driver, PCI enumeration and AHCI setup would happen here. */
    return 0;
}

int sata_read_block(uint32_t lba, uint8_t *buf, size_t count) {
    return block_read(lba, buf, count);
}

int sata_write_block(uint32_t lba, const uint8_t *buf, size_t count) {
    return block_write(lba, buf, count);
}

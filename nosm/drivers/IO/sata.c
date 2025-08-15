#include "sata.h"
#include "io.h"

/* Minimal PIO-based ATA driver.  Only supports 28-bit LBA and
 * sector sized transfers (512 bytes).  It is intentionally small and
 * synchronous as it is primarily used by the block layer to provide
 * disk backed storage for NOSFS.
 */

#define ATA_IO_BASE     0x1F0
#define ATA_REG_DATA    0
#define ATA_REG_ERROR   1
#define ATA_REG_SECCNT  2
#define ATA_REG_LBA0    3
#define ATA_REG_LBA1    4
#define ATA_REG_LBA2    5
#define ATA_REG_HDSEL   6
#define ATA_REG_CMD     7
#define ATA_REG_STATUS  7

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30

static int ata_wait_ready(void)
{
    uint8_t status;
    do {
        status = inb(ATA_IO_BASE + ATA_REG_STATUS);
    } while (status & 0x80); /* BSY */
    if (status & 0x01)       /* ERR */
        return -1;
    return 0;
}

int sata_init(void)
{
    /* Poll the status port to see if a device responds.  This is a very
     * light‑weight check but suffices for our purposes.  If the port
     * returns 0xFF it typically indicates no device.
     */
    if (inb(ATA_IO_BASE + ATA_REG_STATUS) == 0xFF)
        return -1;
    return ata_wait_ready();
}

int sata_read_block(uint32_t lba, uint8_t *buf, size_t count)
{
    if (!buf)
        return -1;
    for (size_t i = 0; i < count; ++i) {
        if (ata_wait_ready() < 0)
            return -1;
        outb(ATA_IO_BASE + ATA_REG_HDSEL, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_IO_BASE + ATA_REG_SECCNT, 1);
        outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)(lba));
        outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ATA_IO_BASE + ATA_REG_CMD, ATA_CMD_READ);
        if (ata_wait_ready() < 0)
            return -1;
        insw(ATA_IO_BASE + ATA_REG_DATA, buf + i * 512, 256);
        lba++;
    }
    return (int)count;
}

int sata_write_block(uint32_t lba, const uint8_t *buf, size_t count)
{
    if (!buf)
        return -1;
    for (size_t i = 0; i < count; ++i) {
        if (ata_wait_ready() < 0)
            return -1;
        outb(ATA_IO_BASE + ATA_REG_HDSEL, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_IO_BASE + ATA_REG_SECCNT, 1);
        outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)(lba));
        outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ATA_IO_BASE + ATA_REG_CMD, ATA_CMD_WRITE);
        if (ata_wait_ready() < 0)
            return -1;
        outsw(ATA_IO_BASE + ATA_REG_DATA, buf + i * 512, 256);
        if (ata_wait_ready() < 0)
            return -1;
        lba++;
    }
    return (int)count;
}

#include "e1000.h"
#include "../IO/pci.h"
#include "../IO/mmio.h"
#include "../IO/serial.h"
#include "../../../user/libc/libc.h"


// Intel e1000 PCI Vendor/Device ID
#define INTEL_VENDOR_ID 0x8086
#define E1000_CLASS     0x02  // Network controller
#define E1000_SUBCLASS  0x00  // Ethernet controller

static volatile uint32_t *regs = NULL;

// Descriptor rings and buffers for basic transmit/receive.
#define RX_DESC_COUNT 16
#define TX_DESC_COUNT 16

struct rx_desc {
    uint64_t addr;
    uint16_t len;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct tx_desc {
    uint64_t addr;
    uint16_t len;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

static struct rx_desc rx_ring[RX_DESC_COUNT];
static struct tx_desc tx_ring[TX_DESC_COUNT];
static uint8_t rx_buf[RX_DESC_COUNT][2048];
static uint8_t tx_buf[TX_DESC_COUNT][2048];
static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

#define REG_RDBAL 0x2800
#define REG_RDBAH 0x2804
#define REG_RDLEN 0x2808
#define REG_RDH   0x2810
#define REG_RDT   0x2818
#define REG_RCTRL 0x0100

#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH   0x3810
#define REG_TDT   0x3818
#define REG_TCTRL 0x0400

static void nic_setup_rx(void) {
    for (int i = 0; i < RX_DESC_COUNT; ++i) {
        rx_ring[i].addr = (uint64_t)(uintptr_t)rx_buf[i];
        rx_ring[i].status = 0;
    }
    mmio_write32((uintptr_t)regs + REG_RDBAL, (uint32_t)(uintptr_t)rx_ring);
    mmio_write32((uintptr_t)regs + REG_RDBAH, 0);
    mmio_write32((uintptr_t)regs + REG_RDLEN, RX_DESC_COUNT * sizeof(struct rx_desc));
    mmio_write32((uintptr_t)regs + REG_RDH, 0);
    mmio_write32((uintptr_t)regs + REG_RDT, RX_DESC_COUNT - 1);
    mmio_write32((uintptr_t)regs + REG_RCTRL, 0x00000002);
}

static void nic_setup_tx(void) {
    for (int i = 0; i < TX_DESC_COUNT; ++i) {
        tx_ring[i].addr = (uint64_t)(uintptr_t)tx_buf[i];
        tx_ring[i].status = 0x01;
    }
    mmio_write32((uintptr_t)regs + REG_TDBAL, (uint32_t)(uintptr_t)tx_ring);
    mmio_write32((uintptr_t)regs + REG_TDBAH, 0);
    mmio_write32((uintptr_t)regs + REG_TDLEN, TX_DESC_COUNT * sizeof(struct tx_desc));
    mmio_write32((uintptr_t)regs + REG_TDH, 0);
    mmio_write32((uintptr_t)regs + REG_TDT, 0);
    mmio_write32((uintptr_t)regs + REG_TCTRL, 0x00000002);
}

// Returns PCI bus/slot packed in int (bus<<8|slot), or -1 if not found
int e1000_init(void) {
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t ven_dev = pci_config_read((uint8_t)bus, slot, func, 0);
                if (ven_dev == 0xFFFFFFFF)
                    continue;
                uint16_t vendor = ven_dev & 0xFFFF;

                if (vendor == INTEL_VENDOR_ID) {
                    uint32_t classcode = pci_config_read((uint8_t)bus, slot, func, 0x08);
                    uint8_t class = (classcode >> 24) & 0xFF;
                    uint8_t subclass = (classcode >> 16) & 0xFF;

                    if (class == E1000_CLASS && subclass == E1000_SUBCLASS) {
                        serial_puts("[net] Intel e1000 NIC detected\n");

                        uint32_t bar0 = pci_config_read((uint8_t)bus, slot, func, 0x10);
                        bar0 &= ~0xF; // mask flags
                        regs = (volatile uint32_t *)(uintptr_t)bar0;

                        char buf[32];
                        buf[0] = '['; buf[1] = 'b'; buf[2] = 'u'; buf[3] = 's';
                        buf[4] = ':'; buf[5] = ' '; buf[6] = '0' + bus; buf[7] = ','; buf[8] = ' ';
                        buf[9] = 's'; buf[10] = 'l'; buf[11] = 'o'; buf[12] = 't';
                        buf[13] = ':'; buf[14] = ' '; buf[15] = '0' + slot; buf[16] = ','; buf[17] = ' ';
                        buf[18] = 'f'; buf[19] = 'n'; buf[20] = ':'; buf[21] = ' '; buf[22] = '0' + func; buf[23] = ']'; buf[24] = '\0';
                        serial_puts(buf);
                        serial_puts("\n");

                        nic_setup_rx();
                        nic_setup_tx();

                        return ((uint8_t)bus << 8) | slot;
                    }
                }
            }
        }
    }
    serial_puts("[net] No Intel NIC found\n");
    return -1;
}

int e1000_get_mac(uint8_t mac[6]) {
    if (!regs)
        return -1;
    uint32_t ral = mmio_read32((uintptr_t)regs + 0x5400);
    uint32_t rah = mmio_read32((uintptr_t)regs + 0x5404);
    mac[0] = ral & 0xFF;
    mac[1] = (ral >> 8) & 0xFF;
    mac[2] = (ral >> 16) & 0xFF;
    mac[3] = (ral >> 24) & 0xFF;
    mac[4] = rah & 0xFF;
    mac[5] = (rah >> 8) & 0xFF;
    return 0;
}

int e1000_transmit(const void *data, size_t len) {
    if (!regs) return -1;
    if (len > sizeof(tx_buf[0])) len = sizeof(tx_buf[0]);
    uint32_t cur = tx_cur;
    memcpy(tx_buf[cur], data, len);
    tx_ring[cur].len = (uint16_t)len;
    tx_ring[cur].cmd = (1 << 0) | (1 << 3); // EOP + RS
    tx_ring[cur].status = 0;
    tx_cur = (tx_cur + 1) % TX_DESC_COUNT;
    mmio_write32((uintptr_t)regs + REG_TDT, tx_cur);
    while (!(tx_ring[cur].status & 0xFF)) { }
    return 0;
}

int e1000_poll(uint8_t *buf, size_t buflen) {
    if (!regs) return -1;
    uint32_t cur = rx_cur;
    if (!(rx_ring[cur].status & 0x01))
        return 0;
    size_t len = rx_ring[cur].len;
    if (len > buflen) len = buflen;
    memcpy(buf, (void *)(uintptr_t)rx_ring[cur].addr, len);
    rx_ring[cur].status = 0;
    rx_cur = (rx_cur + 1) % RX_DESC_COUNT;
    mmio_write32((uintptr_t)regs + REG_RDT, rx_cur);
    return (int)len;
}

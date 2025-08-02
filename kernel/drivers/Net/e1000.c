#include "e1000.h"
#include "../IO/pci.h"
#include "../IO/mmio.h"
#include "../../../user/libc/libc.h"

// Simple VGA print at row 4
static void puts_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80 * 4; // row 4
    while (*s) {
        *vga++ = (0x0F << 8) | *s++;
    }
}

// Intel e1000 PCI Vendor/Device ID
#define INTEL_VENDOR_ID 0x8086
#define E1000_CLASS     0x02  // Network controller
#define E1000_SUBCLASS  0x00  // Ethernet controller

static volatile uint32_t *regs = NULL;

// Returns PCI bus/slot packed in int (bus<<8|slot), or -1 if not found
int e1000_init(void) {
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t ven_dev = pci_config_read(bus, slot, 0, 0);
            if (ven_dev == 0xFFFFFFFF)
                continue;
            uint16_t vendor = ven_dev & 0xFFFF;

            if (vendor == INTEL_VENDOR_ID) {
                uint32_t classcode = pci_config_read(bus, slot, 0, 0x08);
                uint8_t class = (classcode >> 24) & 0xFF;
                uint8_t subclass = (classcode >> 16) & 0xFF;

                if (class == E1000_CLASS && subclass == E1000_SUBCLASS) {
                    puts_vga("[net] Intel e1000 NIC detected\n");

                    uint32_t bar0 = pci_config_read(bus, slot, 0, 0x10);
                    bar0 &= ~0xF; // mask flags
                    regs = (volatile uint32_t *)(uintptr_t)bar0;

                    char buf[32];
                    buf[0] = '['; buf[1] = 'b'; buf[2] = 'u'; buf[3] = 's';
                    buf[4] = ':'; buf[5] = ' '; buf[6] = '0' + bus; buf[7] = ','; buf[8] = ' ';
                    buf[9] = 's'; buf[10] = 'l'; buf[11] = 'o'; buf[12] = 't';
                    buf[13] = ':'; buf[14] = ' '; buf[15] = '0' + slot; buf[16] = ']'; buf[17] = '\0';
                    puts_vga(buf);

                    return (bus << 8) | slot;
                }
            }
        }
    }
    puts_vga("[net] No Intel NIC found\n");
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

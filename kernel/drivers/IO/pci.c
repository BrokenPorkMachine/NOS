#include "io.h"
#include "pci.h"

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                           (func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(PCI_ADDRESS_PORT, address);
    return inl(PCI_DATA_PORT);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                           (func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(PCI_ADDRESS_PORT, address);
    outl(PCI_DATA_PORT, value);
}

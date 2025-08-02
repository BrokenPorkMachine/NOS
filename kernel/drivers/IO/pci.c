#include "io.h"
#include "pci.h"

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                           (func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outb(PCI_ADDRESS_PORT, address & 0xFF);
    outb(PCI_ADDRESS_PORT + 1, (address >> 8) & 0xFF);
    outb(PCI_ADDRESS_PORT + 2, (address >> 16) & 0xFF);
    outb(PCI_ADDRESS_PORT + 3, (address >> 24) & 0xFF);
    uint32_t value = 0;
    value |= inb(PCI_DATA_PORT);
    value |= inb(PCI_DATA_PORT + 1) << 8;
    value |= inb(PCI_DATA_PORT + 2) << 16;
    value |= inb(PCI_DATA_PORT + 3) << 24;
    return value;
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                           (func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outb(PCI_ADDRESS_PORT, address & 0xFF);
    outb(PCI_ADDRESS_PORT + 1, (address >> 8) & 0xFF);
    outb(PCI_ADDRESS_PORT + 2, (address >> 16) & 0xFF);
    outb(PCI_ADDRESS_PORT + 3, (address >> 24) & 0xFF);
    outb(PCI_DATA_PORT, value & 0xFF);
    outb(PCI_DATA_PORT + 1, (value >> 8) & 0xFF);
    outb(PCI_DATA_PORT + 2, (value >> 16) & 0xFF);
    outb(PCI_DATA_PORT + 3, (value >> 24) & 0xFF);
}

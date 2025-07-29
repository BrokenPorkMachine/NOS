#include "../IO/io.h"
#include "../IO/pci.h"
#include "e1000.h"
#include "../src/libc.h"

static void puts_vga(const char *s) {
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80*4; // row 4
    while (*s) {
        *vga++ = (0x0F << 8) | *s++;
    }
}

void e1000_init(void) {
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t ven_dev = pci_config_read(bus, slot, 0, 0);
            if (ven_dev == 0xFFFFFFFF)
                continue;
            uint16_t vendor = ven_dev & 0xFFFF;
            uint16_t device = (ven_dev >> 16) & 0xFFFF;
            if (vendor == 0x8086) {
                uint32_t classcode = pci_config_read(bus, slot, 0, 0x08);
                uint8_t class = (classcode >> 24) & 0xFF;
                uint8_t subclass = (classcode >> 16) & 0xFF;
                if (class == 0x02 && subclass == 0x00) {
                    puts_vga("[net] Intel NIC detected\n");
                    return;
                }
            }
        }
    }
}

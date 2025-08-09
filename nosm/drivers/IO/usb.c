#include <stdint.h>
#include "usb.h"
#include "pci.h"
#include "serial.h"

/* Simple PCI scan for USB controllers. Real hardware handling
   remains to be implemented. */

void usb_init(void) {
    serial_puts("USB: scanning PCI bus for controllers\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vend_dev = pci_config_read(bus, slot, func, 0);
                if ((vend_dev & 0xFFFF) == 0xFFFF)
                    continue; /* Device doesn't exist */

                uint32_t class_reg = pci_config_read(bus, slot, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;

                if (class_code == 0x0C && subclass == 0x03) {
                    serial_puts("USB: controller at B:");
                    serial_puthex(bus);
                    serial_puts(" S:");
                    serial_puthex(slot);
                    serial_puts(" F:");
                    serial_puthex(func);
                    serial_puts("\n");
                }
            }
        }
    }
}

void usb_poll(void) {
    /* Placeholder for polling USB events */
}


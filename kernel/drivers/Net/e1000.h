#pragma once
#include <stdint.h>

// Initialize and detect Intel e1000 (or compatible) NIC on PCI bus.
// Prints status to VGA and returns the PCI bus/slot/function or -1 on failure.
int e1000_init(void);
// Retrieve MAC address into the provided 6-byte buffer.
int e1000_get_mac(uint8_t mac[6]);

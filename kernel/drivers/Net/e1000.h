#pragma once
#include <stdint.h>
#include <stddef.h>

// Initialize and detect Intel e1000 (or compatible) NIC on PCI bus.
// Prints status to VGA and returns the PCI bus/slot/function or -1 on failure.
int e1000_init(void);
// Retrieve MAC address into the provided 6-byte buffer.
int e1000_get_mac(uint8_t mac[6]);

// Transmit a raw Ethernet frame.
int e1000_transmit(const void *data, size_t len);
// Poll for a received frame.  Returns length of frame or 0 if none available.
int e1000_poll(uint8_t *buf, size_t buflen);

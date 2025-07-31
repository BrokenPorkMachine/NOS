#pragma once
#include <stdint.h>

// Initialize and detect Intel e1000 (or compatible) NIC on PCI bus.
// Prints status to VGA and returns the PCI bus/slot/function or -1 on failure.
int e1000_init(void);

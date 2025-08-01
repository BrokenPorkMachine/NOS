#ifndef NET_STACK_H
#define NET_STACK_H
#include <stddef.h>
// Simple loopback network stack used by the user space servers.  It now
// supports a small number of logical "ports" so that multiple servers can
// exchange data without clobbering each other.

void net_init(void);

// Send data on a given port.  Returns the number of bytes queued.
int net_send(unsigned port, const void *data, size_t len);

// Receive data from a given port.  Returns the number of bytes read.
int net_receive(unsigned port, void *buf, size_t buflen);
#endif // NET_STACK_H

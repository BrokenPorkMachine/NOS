#ifndef NET_STACK_H
#define NET_STACK_H
#include <stddef.h>
#include <stdint.h>
// Simple loopback network stack used by the user space servers.  It now
// supports a small number of logical "ports" so that multiple servers can
// exchange data without clobbering each other.  When hardware is present the
// same interfaces are used to send and receive real Ethernet frames.

void net_init(void);

// Send data on a given port.  Returns the number of bytes queued.
int net_send(unsigned port, const void *data, size_t len);

// Receive data from a given port.  Returns the number of bytes read.
int net_receive(unsigned port, void *buf, size_t buflen);

// ---- Socket style API ---------------------------------------------------

typedef enum {
    NET_SOCK_DGRAM = 1,
    NET_SOCK_STREAM = 2
} net_socket_type_t;

int net_socket_open(uint16_t port, net_socket_type_t type);
int net_socket_close(int sock);
int net_socket_send(int sock, const void *data, size_t len);
int net_socket_recv(int sock, void *buf, size_t len);

// Poll hardware NIC and dispatch incoming frames.
void net_poll(void);

// Configure or query the IPv4 address used by the stack.
uint32_t net_get_ip(void);
void net_set_ip(uint32_t ip);

// Retrieve the MAC address currently configured for the stack.
void net_get_mac(uint8_t out[6]);

// Convenience helpers for transmitting IPv4 packets.
int net_send_ipv4_udp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                      const void *data, size_t len);
int net_send_ipv4_tcp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                      const void *data, size_t len);

#endif // NET_STACK_H

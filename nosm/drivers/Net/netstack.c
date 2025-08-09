#include "netstack.h"
#include "e1000.h"
#include "../IO/serial.h"
#include <stdint.h>
#include <string.h>

// Basic network stack with optional hardware backing.  Frames received from
// the NIC are decoded and queued into small per-port buffers so that user
// servers can continue to interact through a simple IPC style API.

#define NET_PORTS   8
#define NETBUF_SIZE 512

// Socket bookkeeping.  Each port can be bound by one socket at a time.
static uint8_t socket_type[NET_PORTS];

static uint8_t netbuf[NET_PORTS][NETBUF_SIZE];
static size_t head[NET_PORTS];
static size_t tail[NET_PORTS];
static size_t count[NET_PORTS];
static uint8_t our_mac[6];
static uint32_t ip_addr = 0x0A00020F; // default 10.0.2.15

static uint16_t swap16(uint16_t x) { return (x >> 8) | (x << 8); }
static uint32_t swap32(uint32_t x) { return ((uint32_t)swap16(x & 0xFFFF) << 16) | swap16(x >> 16); }
#define htons(x) swap16(x)
#define ntohs(x) swap16(x)
#define htonl(x) swap32(x)
#define ntohl(x) swap32(x)

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

struct ipv4_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed));

struct ipv6_hdr {
    uint32_t ver_tc_flow;
    uint16_t payload_len;
    uint8_t next_hdr;
    uint8_t hop_limit;
    uint8_t src[16];
    uint8_t dst[16];
} __attribute__((packed));

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed));

struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t offset_flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed));

struct pseudo_hdr {
    uint32_t src;
    uint32_t dst;
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((packed));

static uint32_t checksum_partial(uint32_t sum, const void *buf, size_t len) {
    const uint8_t *data = (const uint8_t *)buf;
    while (len > 1) {
        sum += ((uint32_t)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += ((uint32_t)data[0] << 8);
    }
    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint16_t checksum(const void *buf, size_t len) {
    return checksum_finish(checksum_partial(0, buf, len));
}

static size_t netbuf_avail(unsigned p) {
    return count[p];
}

void net_init(void) {
    serial_puts("[net] initializing network stack\n");
    int nic = e1000_init();
    if (nic < 0) {
        serial_puts("[net] no supported NIC found\n");
    } else {
        serial_puts("[net] NIC ready\n");
        if (e1000_get_mac(our_mac) == 0) {
            char buf[32];
            const char *hex = "0123456789ABCDEF";
            int p = 0;
            for (int i = 0; i < 6; ++i) {
                buf[p++] = hex[(our_mac[i] >> 4) & 0xF];
                buf[p++] = hex[our_mac[i] & 0xF];
                if (i < 5) buf[p++] = ':';
            }
            buf[p] = '\0';
            serial_puts("[net] MAC ");
            serial_puts(buf);
            serial_puts("\n");
        }
    }
    for (unsigned i = 0; i < NET_PORTS; ++i)
        head[i] = tail[i] = count[i] = socket_type[i] = 0;
}

int net_send(unsigned port, const void *data, size_t len) {
    if (port >= NET_PORTS || socket_type[port] == 0)
        return 0;
    const uint8_t *d = (const uint8_t *)data;
    size_t space = NETBUF_SIZE - netbuf_avail(port);
    if (len > space)
        len = space; // drop excess
    for (size_t i = 0; i < len; ++i) {
        netbuf[port][head[port]++] = d[i];
        if (head[port] >= NETBUF_SIZE)
            head[port] = 0;
    }
    count[port] += len;
    return (int)len;
}

int net_receive(unsigned port, void *buf, size_t buflen) {
    if (port >= NET_PORTS || socket_type[port] == 0)
        return 0;
    uint8_t *b = (uint8_t *)buf;
    size_t avail = netbuf_avail(port);
    if (avail == 0)
        return 0;
    if (buflen < avail)
        avail = buflen;
    for (size_t i = 0; i < avail; ++i) {
        b[i] = netbuf[port][tail[port]++];
        if (tail[port] >= NETBUF_SIZE)
            tail[port] = 0;
    }
    count[port] -= avail;
    return (int)avail;
}

uint32_t net_get_ip(void) {
    return ip_addr;
}

void net_set_ip(uint32_t ip) {
    ip_addr = ip;
}

// ---- Socket style API ---------------------------------------------------

int net_socket_open(uint16_t port, net_socket_type_t type) {
    if (port >= NET_PORTS) return -1;
    if (socket_type[port] != 0) return -1; // already bound
    socket_type[port] = (uint8_t)type;
    head[port] = tail[port] = count[port] = 0;
    return (int)port;
}

int net_socket_close(int sock) {
    if (sock < 0 || sock >= (int)NET_PORTS) return -1;
    socket_type[sock] = 0;
    head[sock] = tail[sock] = count[sock] = 0;
    return 0;
}

int net_socket_send(int sock, const void *data, size_t len) {
    if (sock < 0) return -1;
    return net_send((unsigned)sock, data, len);
}

int net_socket_recv(int sock, void *buf, size_t len) {
    if (sock < 0) return -1;
    return net_receive((unsigned)sock, buf, len);
}

// ---- Protocol handlers --------------------------------------------------

static void enqueue_port(uint16_t port, const uint8_t *data, size_t len) {
    if (port >= NET_PORTS) return;
    net_send(port, data, len);
}

static void handle_udp(const uint8_t *pkt, size_t len) {
    if (len < sizeof(struct udp_hdr)) return;
    const struct udp_hdr *u = (const struct udp_hdr *)pkt;
    uint16_t dst = ntohs(u->dst_port);
    size_t payload_len = ntohs(u->len);
    if (payload_len < sizeof(struct udp_hdr)) return;
    payload_len -= sizeof(struct udp_hdr);
    if (payload_len > len - sizeof(struct udp_hdr))
        payload_len = len - sizeof(struct udp_hdr);
    enqueue_port(dst % NET_PORTS, pkt + sizeof(struct udp_hdr), payload_len);
}

static void handle_tcp(const uint8_t *pkt, size_t len) {
    if (len < sizeof(struct tcp_hdr)) return;
    const struct tcp_hdr *t = (const struct tcp_hdr *)pkt;
    uint16_t dst = ntohs(t->dst_port);
    uint16_t offset = ((ntohs(t->offset_flags) >> 12) & 0xF) * 4;
    if (offset > len) return;
    size_t payload_len = len - offset;
    enqueue_port(dst % NET_PORTS, pkt + offset, payload_len);
}

static void handle_ipv4(const uint8_t *pkt, size_t len) {
    if (len < sizeof(struct ipv4_hdr)) return;
    const struct ipv4_hdr *ip = (const struct ipv4_hdr *)pkt;
    size_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl > len) return;
    size_t total = ntohs(ip->len);
    if (total > len) total = len;
    const uint8_t *payload = pkt + ihl;
    size_t paylen = total - ihl;
    if (ip->proto == 17) {
        handle_udp(payload, paylen);
    } else if (ip->proto == 6) {
        handle_tcp(payload, paylen);
    }
}

static void handle_ipv6(const uint8_t *pkt, size_t len) {
    if (len < sizeof(struct ipv6_hdr)) return;
    const struct ipv6_hdr *ip6 = (const struct ipv6_hdr *)pkt;
    uint8_t next = ip6->next_hdr;
    const uint8_t *payload = pkt + sizeof(struct ipv6_hdr);
    size_t paylen = ntohs(ip6->payload_len);
    if (paylen > len - sizeof(struct ipv6_hdr))
        paylen = len - sizeof(struct ipv6_hdr);
    if (next == 17) {
        handle_udp(payload, paylen);
    } else if (next == 6) {
        handle_tcp(payload, paylen);
    }
}

static void handle_arp(const uint8_t *pkt, size_t len) {
    if (len < sizeof(struct arp_hdr)) return;
    const struct arp_hdr *a = (const struct arp_hdr *)pkt;
    if (ntohs(a->oper) != 1) return; // only handle requests
    if (ntohl(a->tpa) != ip_addr) return; // not for us

    uint8_t frame[sizeof(struct eth_hdr) + sizeof(struct arp_hdr)];
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    struct arp_hdr *r = (struct arp_hdr *)(frame + sizeof(struct eth_hdr));

    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = a->sha[i];
        eth->src[i] = our_mac[i];
        r->tha[i] = a->sha[i];
        r->sha[i] = our_mac[i];
    }
    eth->type = htons(0x0806);

    r->htype = htons(1);
    r->ptype = htons(0x0800);
    r->hlen = 6;
    r->plen = 4;
    r->oper = htons(2); // reply
    r->spa = htonl(ip_addr);
    r->tpa = a->spa;

    e1000_transmit(frame, sizeof(frame));
}

static void handle_frame(const uint8_t *frame, size_t len) {
    if (len < sizeof(struct eth_hdr)) return;
    const struct eth_hdr *eth = (const struct eth_hdr *)frame;
    uint16_t type = ntohs(eth->type);
    const uint8_t *payload = frame + sizeof(struct eth_hdr);
    size_t paylen = len - sizeof(struct eth_hdr);
    if (type == 0x0800) {
        handle_ipv4(payload, paylen);
    } else if (type == 0x86DD) {
        handle_ipv6(payload, paylen);
    } else if (type == 0x0806) {
        handle_arp(payload, paylen);
    }
}

void net_poll(void) {
    uint8_t buf[1600];
    int n;
    while ((n = e1000_poll(buf, sizeof(buf))) > 0) {
        handle_frame(buf, (size_t)n);
    }
}

int net_send_ipv4_udp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                      const void *data, size_t len) {
    uint8_t frame[1600];
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = 0xFF;
        eth->src[i] = our_mac[i];
    }
    eth->type = htons(0x0800);

    struct ipv4_hdr *ip = (struct ipv4_hdr *)(frame + sizeof(struct eth_hdr));
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = htons(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + len);
    ip->id = 0;
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->proto = 17;
    ip->checksum = 0;
    ip->src = htonl(ip_addr);
    ip->dst = htonl(dst_ip);

    struct udp_hdr *udp = (struct udp_hdr *)((uint8_t *)ip + sizeof(struct ipv4_hdr));
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->len = htons(sizeof(struct udp_hdr) + len);
    udp->checksum = 0;

    memcpy((uint8_t *)udp + sizeof(struct udp_hdr), data, len);

    struct pseudo_hdr ph = { ip->src, ip->dst, 0, 17, udp->len };
    uint32_t sum = 0;
    sum = checksum_partial(sum, &ph, sizeof(ph));
    sum = checksum_partial(sum, udp, sizeof(struct udp_hdr) + len);
    udp->checksum = checksum_finish(sum);

    ip->checksum = checksum(ip, sizeof(struct ipv4_hdr));

    size_t total = sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) +
                   sizeof(struct udp_hdr) + len;
    return e1000_transmit(frame, total);
}

int net_send_ipv4_tcp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                      const void *data, size_t len) {
    uint8_t frame[1600];
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = 0xFF;
        eth->src[i] = our_mac[i];
    }
    eth->type = htons(0x0800);

    struct ipv4_hdr *ip = (struct ipv4_hdr *)(frame + sizeof(struct eth_hdr));
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->len = htons(sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr) + len);
    ip->id = 0;
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->proto = 6;
    ip->checksum = 0;
    ip->src = htonl(ip_addr);
    ip->dst = htonl(dst_ip);

    struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + sizeof(struct ipv4_hdr));
    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq = 0;
    tcp->ack = 0;
    tcp->offset_flags = htons((5 << 12) | 0x18); // PSH+ACK
    tcp->window = htons(512);
    tcp->checksum = 0;
    tcp->urgent = 0;

    memcpy((uint8_t *)tcp + sizeof(struct tcp_hdr), data, len);

    struct pseudo_hdr ph = { ip->src, ip->dst, 0, 6,
                             htons(sizeof(struct tcp_hdr) + len) };
    uint32_t sum = 0;
    sum = checksum_partial(sum, &ph, sizeof(ph));
    sum = checksum_partial(sum, tcp, sizeof(struct tcp_hdr) + len);
    tcp->checksum = checksum_finish(sum);

    ip->checksum = checksum(ip, sizeof(struct ipv4_hdr));

    size_t total = sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) +
                   sizeof(struct tcp_hdr) + len;
    return e1000_transmit(frame, total);
}

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../kernel/drivers/Net/netstack.h"

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
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

static uint8_t frame[1600];
static size_t frame_len;

int e1000_transmit(const void *data, size_t len) {
    if (len > sizeof(frame)) len = sizeof(frame);
    memcpy(frame, data, len);
    frame_len = len;
    return (int)len;
}
int e1000_init(void) { return 0; }
int e1000_get_mac(uint8_t *mac) { memset(mac, 0, 6); return 0; }
int e1000_poll(void *buf, size_t len) { (void)buf; (void)len; return 0; }
void serial_puts(const char *s) { (void)s; }

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

static uint16_t htons(uint16_t x) { return (x >> 8) | (x << 8); }

int main(void) {
    net_set_ip(0x0A00020F);
    uint8_t payload[3] = {1,2,3};

    /* Basic socket open/close and loopback send/recv */
    int s = net_socket_open(1, NET_SOCK_DGRAM);
    assert(s == 1);
    const char msg[] = "hello";
    net_socket_send(s, msg, sizeof(msg));
    char rbuf[16];
    int rn = net_socket_recv(s, rbuf, sizeof(rbuf));
    assert(rn == (int)sizeof(msg));
    assert(memcmp(rbuf, msg, sizeof(msg)) == 0);
    /* cannot bind same port twice */
    assert(net_socket_open(1, NET_SOCK_STREAM) == -1);
    net_socket_close(s);
    rn = net_socket_recv(s, rbuf, sizeof(rbuf));
    assert(rn == 0);
    s = net_socket_open(1, NET_SOCK_STREAM);
    assert(s == 1);
    net_socket_close(s);

    net_send_ipv4_udp(0x0A000201, 1111, 2222, payload, sizeof(payload));
    assert(frame_len == sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) +
                          sizeof(struct udp_hdr) + sizeof(payload));
    struct ipv4_hdr *ip = (struct ipv4_hdr *)(frame + sizeof(struct eth_hdr));
    struct udp_hdr *udp = (struct udp_hdr *)((uint8_t *)ip + sizeof(struct ipv4_hdr));
    uint16_t ip_ck = ip->checksum; ip->checksum = 0;
    assert(checksum(ip, sizeof(*ip)) == ip_ck);
    struct pseudo_hdr ph = { ip->src, ip->dst, 0, 17, udp->len };
    uint16_t udp_ck = udp->checksum; udp->checksum = 0;
    uint32_t sum = 0;
    sum = checksum_partial(sum, &ph, sizeof(ph));
    sum = checksum_partial(sum, udp, sizeof(struct udp_hdr) + sizeof(payload));
    assert(checksum_finish(sum) == udp_ck);

    frame_len = 0;
    net_send_ipv4_tcp(0x0A000201, 1111, 2222, payload, sizeof(payload));
    assert(frame_len == sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) +
                          sizeof(struct tcp_hdr) + sizeof(payload));
    ip = (struct ipv4_hdr *)(frame + sizeof(struct eth_hdr));
    struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + sizeof(struct ipv4_hdr));
    ip_ck = ip->checksum; ip->checksum = 0;
    assert(checksum(ip, sizeof(*ip)) == ip_ck);
    struct pseudo_hdr ph2 = { ip->src, ip->dst, 0, 6,
                              htons(sizeof(struct tcp_hdr) + sizeof(payload)) };
    uint16_t tcp_ck = tcp->checksum; tcp->checksum = 0;
    sum = 0;
    sum = checksum_partial(sum, &ph2, sizeof(ph2));
    sum = checksum_partial(sum, tcp, sizeof(struct tcp_hdr) + sizeof(payload));
    assert(checksum_finish(sum) == tcp_ck);
    return 0;
}

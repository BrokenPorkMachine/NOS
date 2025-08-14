#include "virtio.h"
#include "pci.h"
#include "mmio.h"
#include "serial.h"
#include "../../../user/libc/libc.h"

#define VIRTIO_PCI_VENDOR 0x1AF4
#define VIRTIO_PCI_DEVICE_BLK 0x1001

#define VIRTIO_PCI_DEVICE_FEATURES   0x00
#define VIRTIO_PCI_GUEST_FEATURES    0x04
#define VIRTIO_PCI_QUEUE_PFN         0x08
#define VIRTIO_PCI_QUEUE_NUM         0x0C
#define VIRTIO_PCI_QUEUE_SEL         0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY      0x10
#define VIRTIO_PCI_STATUS            0x12
#define VIRTIO_PCI_ISR               0x13
#define VIRTIO_PCI_CONFIG            0x14

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4

#define QUEUE_SIZE 8

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[QUEUE_SIZE];
} __attribute__((packed));

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static uint16_t io_base = 0;
static struct virtq_desc desc[QUEUE_SIZE];
static struct virtq_avail avail;
static struct virtq_used used;
static uint8_t status_bytes[QUEUE_SIZE];

static inline void outb(uint16_t port, uint8_t val){ asm volatile("outb %0,%1"::"a"(val),"Nd"(port)); }
static inline void outw(uint16_t port, uint16_t val){ asm volatile("outw %0,%1"::"a"(val),"Nd"(port)); }
static inline void outl(uint16_t port, uint32_t val){ asm volatile("outl %0,%1"::"a"(val),"Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t ret; asm volatile("inb %1,%0":"=a"(ret):"Nd"(port)); return ret; }
static inline uint16_t inw(uint16_t port){ uint16_t ret; asm volatile("inw %1,%0":"=a"(ret):"Nd"(port)); return ret; }

static int virtio_find_blk(void){
    for (uint16_t bus=0; bus<256; ++bus){
        for (uint8_t slot=0; slot<32; ++slot){
            for (uint8_t func=0; func<8; ++func){
                uint32_t id = pci_config_read(bus, slot, func, 0);
                if (id==0xffffffff) continue;
                if ((id & 0xFFFF)==VIRTIO_PCI_VENDOR){
                    uint16_t dev = (id>>16)&0xFFFF;
                    if (dev==VIRTIO_PCI_DEVICE_BLK){
                        uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                        io_base = (uint16_t)(bar0 & ~0x7);
                        serial_puts("[virtio] virtio-blk detected\n");
                        return 0;
                    }
                }
            }
        }
    }
    return -1;
}

int virtio_blk_init(void){
    if (virtio_find_blk() < 0)
        return -1;

    outb(io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    outb(io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    uint16_t qsz = inw(io_base + VIRTIO_PCI_QUEUE_NUM);
    if (qsz < QUEUE_SIZE)
        return -1;
    outw(io_base + VIRTIO_PCI_QUEUE_SEL, 0);
    memset(&desc, 0, sizeof(desc));
    memset(&avail, 0, sizeof(avail));
    memset(&used, 0, sizeof(used));
    outl(io_base + VIRTIO_PCI_QUEUE_PFN, ((uint32_t)(uintptr_t)&desc) >> 12);
    outb(io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);
    return 0;
}

int virtio_blk_read(uint64_t sector, void *buf){
    if (!io_base) return -1;
    struct virtio_blk_req req = { .type = 0, .reserved = 0, .sector = sector };
    uint16_t idx = avail.idx % QUEUE_SIZE;
    desc[idx].addr = (uint64_t)(uintptr_t)&req;
    desc[idx].len = sizeof(req);
    desc[idx].flags = 0x0002; // NEXT
    desc[idx].next = (idx+1)%QUEUE_SIZE;

    uint16_t data_idx = (idx+1)%QUEUE_SIZE;
    desc[data_idx].addr = (uint64_t)(uintptr_t)buf;
    desc[data_idx].len = 512;
    desc[data_idx].flags = 0x0002; // NEXT
    desc[data_idx].next = (idx+2)%QUEUE_SIZE;

    uint16_t status_idx = (idx+2)%QUEUE_SIZE;
    desc[status_idx].addr = (uint64_t)(uintptr_t)&status_bytes[idx];
    desc[status_idx].len = 1;
    desc[status_idx].flags = 0;

    avail.ring[idx] = idx;
    avail.idx++;
    outw(io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);

    while (used.idx == 0) { }
    used.idx = 0;
    return status_bytes[idx] == 0 ? 0 : -1;
}


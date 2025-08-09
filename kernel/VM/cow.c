#include "paging_adv.h"
#include "pmm_buddy.h"
#include "numa.h"
#include "../../nosm/drivers/IO/serial.h"
#include "../../user/libc/libc.h"
#include "cow.h"

// ----------- Static State -----------
static uint16_t *refcounts = NULL;
static uint8_t  *cow_flags = NULL;
static uint64_t frames = 0;

// ----------- Core API -----------

void cow_init(uint64_t total_frames) {
    frames = total_frames;
    refcounts = calloc(frames, sizeof(uint16_t));
    cow_flags = calloc(frames, sizeof(uint8_t));
    if (!refcounts || !cow_flags) {
        serial_puts("[cow] allocation failure\n");
        for(;;) __asm__("hlt");
    }
}

void cow_inc_ref(uint64_t phys) {
    if (!refcounts) return;
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames) {
        if (refcounts[frame] == 0xFFFF)
            serial_puts("[cow] refcount overflow!\n");
        else
            refcounts[frame]++;
    }
}

void cow_dec_ref(uint64_t phys) {
    if (!refcounts) return;
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames && refcounts[frame] > 0)
        refcounts[frame]--;
}

uint16_t cow_refcount(uint64_t phys) {
    if (!refcounts) return 0;
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames) return refcounts[frame];
    return 0;
}

// Calculate buddy order for a page count.
static uint32_t order_for_pages(uint32_t pages) {
    uint32_t order = 0, count = 1;
    while (count < pages) { count <<= 1; order++; }
    return order;
}

// Contiguous allocator backed by buddy system.
void *alloc_pages(uint32_t pages) {
    if (pages == 0)
        return NULL;
    int node = current_cpu_node();
    uint32_t order = order_for_pages(pages);
    return buddy_alloc(order, node, 0);
}

void free_pages(void *addr, uint32_t pages) {
    if (!addr || pages == 0)
        return;
    int node = current_cpu_node();
    uint32_t order = order_for_pages(pages);
    buddy_free(addr, order, node);
}

// --- COW marking/flag helpers ---

// Get the frame index for a VA, optionally returning the physical address.
static int cow_flag_index(uint64_t virt, uint64_t *phys_out) {
    uint64_t phys = paging_virt_to_phys_adv(virt);
    if (!phys) return -1;
    if (phys_out) *phys_out = phys;
    return phys / PAGE_SIZE;
}

void cow_mark(uint64_t virt) {
    uint64_t phys;
    int idx = cow_flag_index(virt, &phys);
    if (idx < 0) return;
    cow_flags[idx] = 1;
    paging_unmap_adv(virt);
    paging_map_adv(virt, phys, PAGE_PRESENT | PAGE_USER, 0, current_cpu_node());
}

void cow_unmark(uint64_t virt) {
    uint64_t phys;
    int idx = cow_flag_index(virt, &phys);
    if (idx < 0) return;
    cow_flags[idx] = 0;
    paging_unmap_adv(virt);
    paging_map_adv(virt, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER, 0, current_cpu_node());
}

int cow_is_marked(uint64_t virt) {
    int idx = cow_flag_index(virt, NULL);
    if (idx < 0) return 0;
    return cow_flags[idx] ? 1 : 0;
}

// Free frame only if refcount is zero.
int cow_free_frame(uint64_t phys) {
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames && refcounts[frame] == 0) {
        buddy_free((void*)phys, 0, current_cpu_node());
        return 1;
    }
    return 0;
}

// ----------- Page Fault Handler (COW + Demand Paging) -----------
void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id) {
    (void)cpu_id; // NUMA-aware policies can use this later
    uint64_t virt = addr & ~(PAGE_SIZE - 1);
    uint64_t phys = paging_virt_to_phys_adv(virt);

    if (!phys) {
        // Demand paging: allocate and map zeroed page
        void *page = buddy_alloc(0, current_cpu_node(), 0);
        if (page) {
            paging_map_adv(virt, (uint64_t)page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER, 0, current_cpu_node());
            memset((void*)virt, 0, PAGE_SIZE);
            cow_inc_ref((uint64_t)page);
            return;
        }
        serial_puts("[cow] buddy_alloc failed in pfault\n");
    } else if ((err & 2) && cow_is_marked(virt)) { // Write fault, COW page
        if (cow_refcount(phys) > 1) {
            void *newp = buddy_alloc(0, current_cpu_node(), 0);
            if (!newp) {
                serial_puts("[cow] buddy_alloc failed in COW\n");
                for(;;) __asm__("hlt");
            }
            memcpy(newp, (void*)phys, PAGE_SIZE);
            cow_dec_ref(phys);
            cow_inc_ref((uint64_t)newp);
            paging_map_adv(virt, (uint64_t)newp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER, 0, current_cpu_node());
        } else {
            cow_unmark(virt); // Only one ref, just remove COW and restore writable
        }
        return;
    }

    serial_puts("[cow] unhandled pfault: addr=0x");
    for(;;) __asm__("hlt");
}

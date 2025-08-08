#include "paging.h"
#include "pmm.h"
#include "../drivers/IO/serial.h"
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

// Robust contiguous allocator, handles non-contiguous fallback.
void *alloc_pages(uint32_t pages) {
    if (pages == 0)
        return NULL;
    uint64_t base = (uint64_t)alloc_page();
    if (!base)
        return NULL;
    for (uint32_t i = 1; i < pages; ++i) {
        uint64_t addr = (uint64_t)alloc_page();
        if (!addr || addr != base + i * PAGE_SIZE) {
            if (addr)
                free_page((void*)addr);
            for (uint32_t j = 0; j < i; ++j)
                free_page((void*)(base + j * PAGE_SIZE));
            return NULL;
        }
    }
    return (void*)base;
}

void free_pages(void *addr, uint32_t pages) {
    if (!addr)
        return;
    uint64_t base = (uint64_t)addr;
    for (uint32_t i = 0; i < pages; ++i)
        free_page((void*)(base + i * PAGE_SIZE));
}

// --- COW marking/flag helpers ---

// Get the frame index for a VA, or -1 if not mapped
static int cow_flag_index(uint64_t virt) {
    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return -1;
    return phys / PAGE_SIZE;
}

void cow_mark(uint64_t virt) {
    int idx = cow_flag_index(virt);
    if (idx < 0) return;
    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return;
    cow_flags[idx] = 1;
    paging_unmap(virt);
    paging_map(virt, phys, PAGE_PRESENT | PAGE_USER);
}

void cow_unmark(uint64_t virt) {
    int idx = cow_flag_index(virt);
    if (idx < 0) return;
    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return;
    cow_flags[idx] = 0;
    paging_unmap(virt);
    paging_map(virt, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
}

int cow_is_marked(uint64_t virt) {
    int idx = cow_flag_index(virt);
    if (idx < 0) return 0;
    return cow_flags[idx] ? 1 : 0;
}

// Free frame only if refcount is zero.
int cow_free_frame(uint64_t phys) {
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames && refcounts[frame] == 0) {
        free_page((void*)phys);
        return 1;
    }
    return 0;
}

// ----------- Page Fault Handler (COW + Demand Paging) -----------
void handle_page_fault(uint64_t err, uint64_t addr) {
    uint64_t virt = addr & ~(PAGE_SIZE - 1);
    uint64_t phys = paging_virt_to_phys(virt);

    if (!phys) {
        // Demand paging: allocate and map zeroed page
        void *page = alloc_page();
        if (page) {
            paging_map(virt, (uint64_t)page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            memset((void*)virt, 0, PAGE_SIZE);
            cow_inc_ref((uint64_t)page);
            return;
        }
        serial_puts("[cow] alloc_page failed in pfault\n");
    } else if ((err & 2) && cow_is_marked(virt)) { // Write fault, COW page
        if (cow_refcount(phys) > 1) {
            void *newp = alloc_page();
            if (!newp) {
                serial_puts("[cow] alloc_page failed in COW\n");
                for(;;) __asm__("hlt");
            }
            memcpy(newp, (void*)phys, PAGE_SIZE);
            cow_dec_ref(phys);
            cow_inc_ref((uint64_t)newp);
            paging_map(virt, (uint64_t)newp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        } else {
            cow_unmark(virt); // Only one ref, just remove COW and restore writable
        }
        return;
    }

    serial_puts("[cow] unhandled pfault: addr=0x");
    // TODO: Optional: print the address as hex
    for(;;) __asm__("hlt");
}

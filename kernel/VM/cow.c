#include "cow.h"
#include "../../user/libc/libc.h"

static uint16_t *refcounts = NULL;
static uint8_t  *cow_flags = NULL;
static uint64_t frames = 0;

void cow_init(uint64_t total_frames) {
    frames = total_frames;
    refcounts = calloc(frames, sizeof(uint16_t));
    cow_flags = calloc(frames, sizeof(uint8_t));
    if (!refcounts || !cow_flags) {
        // allocation failure is fatal in this simple kernel
        for(;;) __asm__("hlt");
    }
}

void cow_inc_ref(uint64_t phys) {
    if (!refcounts) return;
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames) refcounts[frame]++;
}

void cow_dec_ref(uint64_t phys) {
    if (!refcounts) return;
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames && refcounts[frame] > 0) refcounts[frame]--;
}

uint16_t cow_refcount(uint64_t phys) {
    uint64_t frame = phys / PAGE_SIZE;
    if (frame < frames) return refcounts[frame];
    return 0;
}

static int flag_index(uint64_t virt) {
    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return -1;
    return phys / PAGE_SIZE;
}

void cow_mark(uint64_t virt) {
    int idx = flag_index(virt);
    if (idx < 0) return;

    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return;

    cow_flags[idx] = 1;
    paging_unmap(virt);
    paging_map(virt, phys, PAGE_PRESENT | PAGE_USER);
}

void cow_unmark(uint64_t virt) {
    int idx = flag_index(virt);
    if (idx < 0) return;

    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) return;

    cow_flags[idx] = 0;
    paging_unmap(virt);
    paging_map(virt, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
}

int cow_is_marked(uint64_t virt) {
    int idx = flag_index(virt);
    if (idx < 0) return 0;
    return cow_flags[idx];
}

void handle_page_fault(uint64_t err, uint64_t addr) {
    uint64_t virt = addr & ~0xFFFULL;
    uint64_t phys = paging_virt_to_phys(virt);
    if (!phys) {
        // simple demand paging: allocate zero page
        void *page = alloc_page();
        if (page) {
            paging_map(virt, (uint64_t)page,
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            memset((void*)virt, 0, PAGE_SIZE);
            cow_inc_ref((uint64_t)page);
            return;
        }
    }
    if ((err & 2) && phys && cow_is_marked(virt)) {
        if (cow_refcount(phys) > 1) {
            void *newp = alloc_page();
            if (!newp) return;
            memcpy(newp, (void*)phys, PAGE_SIZE);
            cow_dec_ref(phys);
            cow_inc_ref((uint64_t)newp);
            paging_map(virt, (uint64_t)newp,
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        } else {
            cow_unmark(virt);
        }
        return;
    }
    // unhandled fault -> halt
    for(;;) __asm__("hlt");
}

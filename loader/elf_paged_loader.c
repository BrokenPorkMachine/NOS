// ============================================================================
// File: loader/elf_paged_loader.c
// Purpose: Page-mapped ELF64 loader implementation for NitrOS (drop-in)
// ============================================================================
#include "elf_paged_loader.h"
#include <string.h>

// ---- Kernel VM/PMM hooks (provided by your kernel) -------------------------
extern void*     vmm_reserve(size_t size, size_t align);        // reserve VA range
extern int       vmm_map(void* va, uintptr_t pa, int prot);     // map 1 page VA->PA
extern void      vmm_prot(void* va, size_t size, int prot);     // change protections
extern void      vmm_unmap(void* va, size_t size);              // unmap VA range
extern int       vmm_is_mapped_x(void* va);                     // VA has execute perm?
extern uintptr_t pmm_alloc_page(void);                          // 4K physical page

// Optional logging (weakly linked)
__attribute__((weak)) void kputs(const char* s) { (void)s; }
__attribute__((weak)) void kprintf(const char* s, ...) { (void)s; }

// ---- Minimal ELF64 structures ---------------------------------------------
#define PT_LOAD 1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {                // Elf64_Phdr (packed subset)
    uint32_t p_type;             // PT_*
    uint32_t p_flags;            // PF_R=4 PF_W=2 PF_X=1
    uint64_t p_offset;           // file offset
    uint64_t p_vaddr;            // preferred VA (ignored; we place anywhere)
    uint64_t p_paddr;            // phys addr (unused)
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;            // page alignment
} __attribute__((packed)) phdr64_t;

typedef struct {                // Elf64_Ehdr (packed subset)
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;            // entry RVA (relative to image base assumptions)
    uint64_t e_phoff;            // program header table file offset
    uint64_t e_shoff;            // (unused)
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed)) ehdr64_t;

// ---- Internals -------------------------------------------------------------
#define PAGE_SZ 4096ULL
static inline size_t up(size_t x, size_t a){ return (x + (a-1)) & ~(a-1); }
static inline int prot_from_flags(uint32_t f){ int p=0; if(f&PF_X)p|=1; if(f&PF_W)p|=2; if(f&PF_R)p|=4; return p; }

static int map_segment_paged(const uint8_t* file, size_t file_sz,
                             uint64_t f_off, uint64_t f_sz,
                             uint64_t mem_sz, uint32_t flags,
                             void** out_va, size_t* out_sz)
{
    size_t map_sz = up((size_t)mem_sz, PAGE_SZ);
    void* base = vmm_reserve(map_sz, PAGE_SZ);
    if (!base) return -12; // -ENOMEM

    // Map RW temporarily, we’ll set final perms after copy/zero
    size_t mapped = 0;
    for (; mapped < map_sz; mapped += PAGE_SZ){
        uintptr_t pa = pmm_alloc_page();
        if (!pa) { vmm_unmap(base, mapped); return -12; }
        if (vmm_map((uint8_t*)base + mapped, pa, /*RW temp*/ (4|2)) != 0){
            vmm_unmap(base, mapped);
            return -5;
        }
    }

    if (f_off + f_sz > file_sz) { vmm_unmap(base, map_sz); return -5; }
    memcpy(base, file + f_off, (size_t)f_sz);
    if (mem_sz > f_sz) memset((uint8_t*)base + f_sz, 0, (size_t)(mem_sz - f_sz));

    vmm_prot(base, map_sz, prot_from_flags(flags));
    if (out_va) *out_va = base; 
    if (out_sz) *out_sz = map_sz;
    return 0;
}

int elf_load_paged(const uint8_t* file, size_t file_sz, elf_map_result_t* out)
{
    if (!file || file_sz < sizeof(ehdr64_t) || !out) return -22; // -EINVAL
    const ehdr64_t* E = (const ehdr64_t*)file;
    if (E->e_phoff + (size_t)E->e_phnum * E->e_phentsize > file_sz) return -22;

    memset(out, 0, sizeof(*out));

    // Map each PT_LOAD
    for (uint16_t i=0; i<E->e_phnum; i++){
        const phdr64_t* P = (const phdr64_t*)(file + E->e_phoff + (size_t)i * E->e_phentsize);
        if (P->p_type != PT_LOAD) continue;
        void* va = 0; size_t sz = 0;
        int rc = map_segment_paged(file, file_sz, P->p_offset, P->p_filesz, P->p_memsz, P->p_flags, &va, &sz);
        if (rc){
            elf_unmap(out);
            return rc;
        }
        if (P->p_flags & PF_X) { out->base_text = va; out->text_sz = sz; }
        else if (P->p_flags & PF_W) { out->base_rw = va; out->rw_sz = sz; }
        else { out->base_ro = va; out->ro_sz = sz; }
    }

    // Compute entry VA.
    // Many simple ELF agents are linked with base 0x400000; entry is RVA from that base.
    if (!out->base_text) { elf_unmap(out); return -5; }
    uint64_t link_base = 0x400000ULL; // adjust if you support PIE/ET_DYN in the future
    out->entry_va = (uint8_t*)out->base_text + (size_t)(E->e_entry - link_base);

    // Validate entry executable
    if (!vmm_is_mapped_x(out->entry_va)) {
        kputs("[elf] entry VA not executable");
        elf_unmap(out);
        return -22;
    }
    return 0;
}

void elf_unmap(const elf_map_result_t* m)
{
    if (!m) return;
    if (m->base_text && m->text_sz) vmm_unmap(m->base_text, m->text_sz);
    if (m->base_ro   && m->ro_sz)   vmm_unmap(m->base_ro,   m->ro_sz);
    if (m->base_rw   && m->rw_sz)   vmm_unmap(m->base_rw,   m->rw_sz);
}

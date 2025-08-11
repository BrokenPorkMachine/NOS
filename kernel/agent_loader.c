// kernel/agent_loader.c  -- C-only loader with arena fallback
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "VM/kheap.h"     // kalloc()
#include "agent.h"        // may or may not declare register_and_spawn()
#include "printf.h"       // serial_printf()

// Fallback prototypes in case headers don't declare them
#ifndef HAVE_SERIAL_PRINTF
extern int serial_printf(const char *fmt, ...);
#endif
#ifndef HAVE_REGISTER_AND_SPAWN
extern int register_and_spawn(const char *name, void *entry, int prio);
#endif

// ---- Minimal ELF64 bits ----
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

typedef struct {
    uint32_t d_tag;
    uint64_t d_val;
} Elf64_Dyn;

#define PT_LOAD       1
#define PT_DYNAMIC    2
#define PT_GNU_RELRO  0x6474e552u

#define DT_NULL     0
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_JMPREL   23
#define DT_PLTREL   20
#define DT_PLTRELSZ 2

#define R_X86_64_RELATIVE 8
#define ELF64_R_TYPE(i)   ((uint32_t)((i) & 0xffffffffu))

// ---- Arena fallback -------------------------------------------------
static __attribute__((aligned(4096))) uint8_t s_loader_arena[512 * 1024];
static size_t s_loader_off = 0;

static void* arena_alloc(size_t bytes, size_t align)
{
    size_t off = (s_loader_off + (align - 1)) & ~(align - 1);
    if (off + bytes > sizeof(s_loader_arena)) return NULL;
    void* p = &s_loader_arena[off];
    s_loader_off = off + bytes;
    return p;
}

static void* kalloc_aligned_or_arena(size_t bytes, size_t align)
{
    // Try kernel heap first
    void* raw = kalloc(bytes + align);
    if (raw) {
        uintptr_t up = (uintptr_t)raw;
        uintptr_t ap = (up + (align - 1)) & ~(uintptr_t)(align - 1);
        return (void*)ap;
    }
    serial_printf("[loader] kalloc %zu failed, trying arena\n", (unsigned long)bytes);
    return arena_alloc(bytes, align);
}

// ---- Helpers --------------------------------------------------------
static int is_elf64(const void* img, size_t sz, const Elf64_Ehdr** out_eh)
{
    if (sz < sizeof(Elf64_Ehdr)) return 0;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)img;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return 0;
    if (eh->e_ident[4] != 2 /*ELFCLASS64*/) return 0;
    if (eh->e_machine  != 62 /*x86_64*/)    return 0;
    *out_eh = eh;
    return 1;
}

static void dump_bytes(const uint8_t* p, size_t n, uint64_t where)
{
    size_t i, j;
    serial_printf("[loader] dumping 64B around entry 0x%016lx\n", (unsigned long)where);
    for (i = 0; i < n; i += 16) {
        serial_printf("[dump] %016lx : ", (unsigned long)(where + i));
        for (j = 0; j < 16 && i + j < n; ++j) serial_printf("%02x ", p[i + j]);
        serial_printf(" |");
        for (j = 0; j < 16 && i + j < n; ++j) {
            unsigned c = p[i + j];
            serial_printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        serial_printf("|\n");
    }
}

static size_t apply_rela_table(uint8_t* load_base, uint64_t lo_for_exec,
                               const void* img, uint64_t tab, uint64_t sz_bytes)
{
    size_t applied = 0;
    if (!tab || !sz_bytes) return 0;

    const Elf64_Rela* r = (const Elf64_Rela*)((const uint8_t*)img + tab);
    size_t n = sz_bytes / sizeof(Elf64_Rela);

    for (size_t i = 0; i < n; ++i) {
        uint32_t type = ELF64_R_TYPE(r[i].r_info);
        if (type == R_X86_64_RELATIVE) {
            uint64_t where_off = r[i].r_offset - lo_for_exec;
            uint64_t* where = (uint64_t*)(load_base + where_off);
            *where = (uint64_t)(load_base + r[i].r_addend);
            ++applied;
        }
    }
    return applied;
}

// ET_EXEC relocations: RELATIVE only
static size_t apply_relocations_rela(uint8_t* load_base, uint64_t lo_for_exec,
                                     const Elf64_Ehdr* eh, const void* img, size_t sz)
{
    (void)sz;
    const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);
    const Elf64_Phdr* dyn = NULL;
    uint16_t i;

    for (i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) { dyn = &ph[i]; break; }
    }
    if (!dyn) return 0;

    const Elf64_Dyn* d = (const Elf64_Dyn*)((const uint8_t*)img + dyn->p_offset);
    size_t dcount = dyn->p_filesz / sizeof(Elf64_Dyn);

    uint64_t rela = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
    uint64_t jmprel = 0, pltrel_sz = 0;

    for (i = 0; i < dcount; ++i) {
        switch (d[i].d_tag) {
            case DT_RELA:     rela      = d[i].d_val; break;
            case DT_RELASZ:   rela_sz   = d[i].d_val; break;
            case DT_RELAENT:  rela_ent  = d[i].d_val; break;
            case DT_JMPREL:   jmprel    = d[i].d_val; break;
            case DT_PLTRELSZ: pltrel_sz = d[i].d_val; break;
            default: break;
        }
    }

    size_t n1 = apply_rela_table(load_base, lo_for_exec, img, rela,   rela_sz);
    size_t n2 = apply_rela_table(load_base, lo_for_exec, img, jmprel, pltrel_sz);
    return n1 + n2;
}

// ---- ELF map + spawn ------------------------------------------------
static int elf_map_and_spawn(const char* path, const void* img, size_t sz, int prio)
{
    (void)prio;
    const Elf64_Ehdr* eh = NULL;
    if (!is_elf64(img, sz, &eh)) return -2;

    serial_printf("[loader] ELF: type=%u phnum=%u e_entry=0x%lx\n",
                  (unsigned)eh->e_type, (unsigned)eh->e_phnum, (unsigned long)eh->e_entry);

    const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);

    // Compute span of PT_LOAD
    uint64_t lo = UINT64_MAX, hi = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_memsz == 0) continue;
        if (ph[i].p_vaddr < lo) lo = ph[i].p_vaddr;
        if (ph[i].p_vaddr + ph[i].p_memsz > hi) hi = ph[i].p_vaddr + ph[i].p_memsz;
    }
    if (lo == UINT64_MAX || hi <= lo) return -3;

    uint64_t span = hi - lo;
    serial_printf("[loader] span: lo=0x%lx hi=0x%lx span=%lu\n",
                  (unsigned long)lo, (unsigned long)hi, (unsigned long)span);

    uint8_t* load_base = (uint8_t*)kalloc_aligned_or_arena((size_t)span, 0x1000);
    if (!load_base) {
        serial_printf("[loader] kalloc %lu failed\n", (unsigned long)span);
        return -7; // ENOMEM
    }

    // Map segments
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;

        const uint64_t off   = ph[i].p_offset;
        const uint64_t files = ph[i].p_filesz;
        const uint64_t mems  = ph[i].p_memsz;
        const uint64_t va    = ph[i].p_vaddr;

        uint8_t* dst = load_base + (va - lo);
        if (files) memcpy(dst, (const uint8_t*)img + off, (size_t)files);
        if (mems > files) memset(dst + files, 0, (size_t)(mems - files));

        serial_printf("[loader] PT_LOAD: vaddr=0x%lx file=%lu mem=%lu fl=0x%x -> [%p..%p)\n",
                      (unsigned long)va, (unsigned long)files, (unsigned long)mems,
                      (unsigned)ph[i].p_flags, dst, dst + mems);
    }

    // Apply RELA (relative) if present
    {
        size_t nrel = apply_relocations_rela(load_base, lo, eh, img, sz);
        serial_printf("[loader] relocations: applied %zu R_X86_64_RELATIVE\n", nrel);
    }

    // Runtime entry
    uint64_t runtime = (uint64_t)(load_base + (eh->e_entry - lo));
    const uint8_t* ebytes = (const uint8_t*)runtime;
    serial_printf("[loader] runtime entry (ET_EXEC) = 0x%016lx\n", (unsigned long)runtime);
    serial_printf("[loader] entry first bytes: %02x %02x %02x %02x\n",
                  ebytes[0], ebytes[1], ebytes[2], ebytes[3]);

    // 64B window near entry (nice for diagnostics)
    {
        uint64_t start = (runtime >= 32) ? (runtime - 32) : runtime;
        dump_bytes((const uint8_t*)start, 64, start);
    }

    // Launch
    return register_and_spawn(path ? path : "(elf)", (void*)runtime, 200);
}

// ---- Public entry points -------------------------------------------
typedef enum {
    AGENT_FORMAT_ELF = 0,
    AGENT_FORMAT_MACHO2,
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_FLAT,
    AGENT_FORMAT_UNKNOWN
} agent_format_t;

static agent_format_t detect_format(const void* image, size_t size)
{
    (void)size;
    const Elf64_Ehdr* eh = NULL;
    if (is_elf64(image, size, &eh)) return AGENT_FORMAT_ELF;
    return AGENT_FORMAT_UNKNOWN;
}

int load_agent_with_prio(const void* image, size_t size, agent_format_t fmt, int prio)
{
    if (fmt == AGENT_FORMAT_UNKNOWN) fmt = detect_format(image, size);
    if (fmt == AGENT_FORMAT_ELF) return elf_map_and_spawn(NULL, image, size, prio);
    return -5; // ENOTSUP
}

int load_agent(const void* image, size_t size, agent_format_t fmt)
{
    return load_agent_with_prio(image, size, fmt, 200);
}

// used by agent_loader_pub.c (run_from_path)
int load_agent_auto_path(const char* path, const void* image, size_t size)
{
    return elf_map_and_spawn(path, image, size, 200);
}

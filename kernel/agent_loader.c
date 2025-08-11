// kernel/agent_loader.c
// Hardened ELF loader for NOS agents (ET_EXEC & ET_DYN/PIE).
// - Pure C (no lambdas), safe logging, exact entry math, RELA support.
// - Weak-calls idt_guard_init_once() if present, but never depends on it.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   // strtol

#include "agent_loader.h"
#include "VM/kheap.h"   // kalloc
#include "Task/thread.h"
#include "symbols.h"
#include "elf.h"        // project-provided 64-bit ELF types

// Some trees declare serial_printf in a local printf.h; some don’t.
// Forward-declare to avoid implicit warnings in either case.
int serial_printf(const char *fmt, ...);

// Optional guard hook: link-safe even when not compiled in.
__attribute__((weak)) void idt_guard_init_once(void);

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

#define ALIGN_UP(x,a)   (((x) + ((a)-1)) & ~((a)-1))
#define ALIGN_DOWN(x,a) ((x) & ~((a)-1))

// ---- Minimal JSON helpers (for embedded manifest strings) -------------------
// Marked unused so they won’t warn if the build doesn’t reference them.
static __attribute__((unused))
int json_get_str(const char* json, const char* key, char* out, size_t osz) {
    if (!json || !key || !out || osz == 0) return -1;
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return -1;
    p = strchr(p + strlen(pat), ':'); if (!p) return -1;
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < osz - 1) out[i++] = *p++;
    if (*p != '"') return -1;
    out[i] = 0;
    return 0;
}

static __attribute__((unused))
int json_get_int(const char* json, const char* key, long* out) {
    if (!json || !key || !out) return -1;
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return -1;
    p = strchr(p + strlen(pat), ':'); if (!p) return -1;
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    char* end = NULL;
    long v = strtol(p, &end, 0);
    if (end == p) return -1;
    *out = v;
    return 0;
}

// ---- Mapping & relocation ----------------------------------------------------

typedef struct {
    uint8_t* base;          // chosen load base (kernel VA)
    uint64_t lo;            // lowest PT_LOAD vaddr (for ET_EXEC span)
    uint64_t hi;            // highest PT_LOAD end
    uint64_t entry_va;      // runtime entry VA (kernel VA)
    size_t   applied_relative;
} elf_map_result_t;

static void* kalloc_aligned(size_t bytes, size_t align) {
    size_t req = ALIGN_UP(bytes, align ? align : PAGE_SIZE);
    void* p = kalloc(req);
    if (!p) serial_printf("[loader] kalloc %zu failed\n", (unsigned long)bytes);
    return p;
}

static int elf_span(const Elf64_Ehdr* eh, const uint8_t* img, size_t sz,
                    uint64_t* lo, uint64_t* hi) {
    if (eh->e_phoff + (uint64_t)eh->e_phentsize * eh->e_phnum > sz) return -1;
    const Elf64_Phdr* ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    uint64_t min = UINT64_MAX, max = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_vaddr < min) min = ph[i].p_vaddr;
        uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > max) max = end;
    }
    if (min == UINT64_MAX) return -1;
    *lo = ALIGN_DOWN(min, PAGE_SIZE);
    *hi = ALIGN_UP(max, PAGE_SIZE);
    return 0;
}

static void memcpy_safe(void* dst, const void* src, size_t n) { if (n) memcpy(dst, src, n); }
static void bzero_safe(void* dst, size_t n) { if (n) memset(dst, 0, n); }

static const Elf64_Rela*
rela_table_from_va(const Elf64_Ehdr* eh, const Elf64_Phdr* ph,
                   const uint8_t* img, size_t img_sz,
                   uint64_t tab_va, size_t sz_bytes)
{
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t pv = ph[i].p_vaddr;
        uint64_t pe = pv + ph[i].p_filesz;
        if (tab_va >= pv && tab_va + sizeof(Elf64_Rela) <= pe) {
            uint64_t off_in_seg = tab_va - pv;
            uint64_t file_off   = ph[i].p_offset + off_in_seg;
            if (file_off + sz_bytes <= img_sz) {
                return (const Elf64_Rela*)(img + file_off);
            }
            return NULL;
        }
    }
    return NULL;
}

static void apply_rela_table(uint8_t* load_base, uint64_t lo_for_exec,
                             const Elf64_Ehdr* eh, const Elf64_Phdr* ph,
                             const uint8_t* img, size_t img_sz,
                             uint64_t tab_va, uint64_t sz_bytes, size_t* applied_out)
{
    const Elf64_Rela* r = rela_table_from_va(eh, ph, img, img_sz, tab_va, sz_bytes);
    if (!r) return;

    size_t cnt = sz_bytes / sizeof(Elf64_Rela);
    size_t applied = 0;

    for (size_t i = 0; i < cnt; ++i) {
        uint32_t type = (uint32_t)ELF64_R_TYPE(r[i].r_info);
        uint64_t rva  = r[i].r_offset;
        uint64_t add  = r[i].r_addend;

        uint8_t* where = (eh->e_type == ET_EXEC)
            ? (load_base + (rva - lo_for_exec))
            : (load_base + rva);

        switch (type) {
            case R_X86_64_RELATIVE: {
                // *where = base + addend  (for ET_DYN; for ET_EXEC this acts as load bias + addend)
                uint64_t val = (uint64_t)(uintptr_t)(load_base + add);
                memcpy(where, &val, sizeof(val));
                applied++;
            } break;

            case R_X86_64_64:
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                // Static agents shouldn’t need extern symbol binding; ignore safely.
                break;

            default:
                // Unknown reloc: ignore.
                break;
        }
    }

    if (applied_out) *applied_out += applied;
}

static size_t apply_relocations_rela(uint8_t* load_base,
                                     uint64_t lo_for_exec,
                                     const Elf64_Ehdr* eh,
                                     const uint8_t* img, size_t sz)
{
    if (eh->e_phoff + (uint64_t)eh->e_phentsize * eh->e_phnum > sz) return 0;
    const Elf64_Phdr* ph = (const Elf64_Phdr*)(img + eh->e_phoff);

    const Elf64_Phdr* dynph = NULL;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) { dynph = &ph[i]; break; }
    }
    if (!dynph) return 0;

    const Elf64_Dyn* dyn = (const Elf64_Dyn*)(img + dynph->p_offset);
    size_t dyn_cnt = dynph->p_filesz / sizeof(Elf64_Dyn);

    uint64_t rela = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
    uint64_t jmprel = 0, pltrel_sz = 0;

    for (size_t i = 0; i < dyn_cnt; ++i) {
        switch (dyn[i].d_tag) {
            case DT_RELA:     rela      = dyn[i].d_un.d_ptr; break;
            case DT_RELASZ:   rela_sz   = dyn[i].d_un.d_val; break;
            case DT_RELAENT:  rela_ent  = dyn[i].d_un.d_val; break;
            case DT_JMPREL:   jmprel    = dyn[i].d_un.d_ptr; break;
            case DT_PLTRELSZ: pltrel_sz = dyn[i].d_un.d_val; break;
            default: break;
        }
    }

    (void)rela_ent; // not needed since we assume sizeof(Elf64_Rela)

    size_t applied = 0;
    if (rela && rela_sz)
        apply_rela_table(load_base, lo_for_exec, eh, ph, img, sz, rela,   rela_sz,   &applied);
    if (jmprel && pltrel_sz)
        apply_rela_table(load_base, lo_for_exec, eh, ph, img, sz, jmprel, pltrel_sz, &applied);

    return applied;
}

// ---- Core ELF mapping --------------------------------------------------------

static int elf_map(const void* image, size_t size, elf_map_result_t* out) {
    if (!image || size < sizeof(Elf64_Ehdr) || !out) return -1;

    const uint8_t* img = (const uint8_t*)image;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)img;

    if (!(eh->e_ident[0] == 0x7f && eh->e_ident[1] == 'E' &&
          eh->e_ident[2] == 'L' && eh->e_ident[3] == 'F' &&
          eh->e_ident[4] == ELFCLASS64 && eh->e_ident[5] == ELFDATA2LSB)) {
        return -2;
    }
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return -3;
    if (eh->e_phentsize != sizeof(Elf64_Phdr)) return -4;
    if (eh->e_phoff + (uint64_t)eh->e_phentsize * eh->e_phnum > size) return -5;

    const Elf64_Phdr* ph = (const Elf64_Phdr*)(img + eh->e_phoff);
    serial_printf("[loader] ELF: type=%u phnum=%u e_entry=0x%lx\n",
                  (unsigned)eh->e_type, (unsigned)eh->e_phnum, (unsigned long)eh->e_entry);

    uint64_t lo = 0, hi = 0;
    if (elf_span(eh, img, size, &lo, &hi) != 0) return -6;
    size_t span = (size_t)(hi - lo);
    serial_printf("[loader] span: lo=0x%lx hi=0x%lx span=%zu\n",
                  (unsigned long)lo, (unsigned long)hi, (unsigned long)span);

    uint8_t* base = (uint8_t*)kalloc_aligned(span, PAGE_SIZE);
    if (!base) return -7;

    // Copy PT_LOAD segments into place
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t dst_off = (eh->e_type == ET_EXEC) ? (ph[i].p_vaddr - lo) : ph[i].p_vaddr;
        uint8_t*  dst    = base + dst_off;

        serial_printf("[loader] PT_LOAD[%u]: vaddr=0x%lx file=%lu mem=%lu fl=0x%lx -> [%p..%p)\n",
                      (unsigned)i,
                      (unsigned long)ph[i].p_vaddr,
                      (unsigned long)ph[i].p_filesz,
                      (unsigned long)ph[i].p_memsz,
                      (unsigned long)ph[i].p_flags,
                      dst, dst + ph[i].p_memsz);

        if (ph[i].p_filesz) {
            if (ph[i].p_offset + ph[i].p_filesz > size) { return -8; }
            memcpy_safe(dst, img + ph[i].p_offset, (size_t)ph[i].p_filesz);
        }
        size_t bss = (size_t)(ph[i].p_memsz - ph[i].p_filesz);
        if (bss) bzero_safe(dst + ph[i].p_filesz, bss);
    }

    // Apply RELA (if any)
    size_t applied = apply_relocations_rela(base, lo, eh, img, size);
    serial_printf("[loader] relocations: applied %zu R_X86_64_RELATIVE\n", applied);

    // Compute runtime entry VA
    uint64_t entry_rva = eh->e_entry;
    if (eh->e_type == ET_EXEC) {
        out->entry_va = (uint64_t)(uintptr_t)(base + (entry_rva - lo));
        serial_printf("[loader] runtime entry (ET_EXEC) = 0x%lx (e_entry-lo=0x%lx, base=%p)\n",
                      (unsigned long)out->entry_va,
                      (unsigned long)(entry_rva - lo),
                      base);
    } else {
        out->entry_va = (uint64_t)(uintptr_t)(base + entry_rva);
        serial_printf("[loader] runtime entry (PIE) = 0x%lx (base=%p + off=0x%lx)\n",
                      (unsigned long)out->entry_va, base, (unsigned long)entry_rva);
    }

    out->base = base;
    out->lo   = lo;
    out->hi   = hi;
    out->applied_relative = applied;
    return 0;
}

// ---- Public API --------------------------------------------------------------

void* agent_loader_last_entry = NULL;

static int load_agent_elf(const void* image, size_t size, int prio) {
    (void)prio;
    if (idt_guard_init_once) idt_guard_init_once();

    elf_map_result_t r = {0};
    int rc = elf_map(image, size, &r);
    if (rc != 0) return rc;

    // Dump first bytes at entry (helps spot NULL/garbage)
    uint8_t b[16] = {0};
    memcpy_safe(b, (void*)(uintptr_t)r.entry_va, sizeof(b));
    serial_printf("[loader] entry first bytes: %02x %02x %02x %02x\n", b[0], b[1], b[2], b[3]);

    // Small window dump around entry
    serial_printf("[loader] dumping 64B around entry 0x%016lx\n", (unsigned long)r.entry_va);
    uint8_t* win = (uint8_t*)(uintptr_t)(r.entry_va - 0x20);
    for (int i = 0; i < 4; ++i) {
        uint64_t addr = (uint64_t)(uintptr_t)(win + i*16);
        serial_printf("[dump] %016lx : %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  |................|\n",
                      (unsigned long)addr,
                      win[i*16+0], win[i*16+1], win[i*16+2], win[i*16+3],
                      win[i*16+4], win[i*16+5], win[i*16+6], win[i*16+7],
                      win[i*16+8], win[i*16+9], win[i*16+10], win[i*16+11],
                      win[i*16+12], win[i*16+13], win[i*16+14], win[i*16+15]);
    }

    agent_loader_last_entry = (void*)(uintptr_t)r.entry_va;
    return 0; // regx will spawn from agent_loader_last_entry
}

int load_agent_with_prio(const void *image, size_t size, agent_format_t fmt, int prio) {
    switch (fmt) {
        case AGENT_FORMAT_ELF:
            return load_agent_elf(image, size, prio);
        case AGENT_FORMAT_MACHO2:
        case AGENT_FORMAT_NOSM:
        case AGENT_FORMAT_FLAT:
        default:
            return -38; // ENOSYS-style
    }
}

int load_agent(const void *image, size_t size, agent_format_t fmt) {
    return load_agent_with_prio(image, size, fmt, 200);
}

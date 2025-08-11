// kernel/agent_loader.c
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>          // snprintf
#include <elf.h>            // Elf64_*
#include "printf.h"         // serial_printf, serial_puts/putc
#include "agent_loader.h"   // agent_format_t, prototypes, agent_gate_fn, agent_loader_get_gate
#include "VM/kheap.h"       // kalloc/kfree
#include <stdlib.h>  // for strtol (even in freestanding this is fine as a decl)

// Fallback decls in case printf.h/serial.h didn’t prototype them:
int  serial_printf(const char *fmt, ...);
void serial_puts(const char *s);
void serial_putc(char c);

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uintptr_t align_up_ptr(uintptr_t v, uintptr_t a) { return (v + a - 1) & ~(a - 1); }

static void *kalloc_aligned_or_arena(size_t bytes, size_t align);
static size_t apply_relocations_rela(uint8_t *load_base, uint64_t lo_for_exec,
                                     const Elf64_Ehdr *eh, const void *img, size_t sz);
static int elf_map_and_spawn(const void *img, size_t sz, const char *path, int prio);
//static void dump_bytes(uintptr_t addr, const uint8_t *p, size_t n);
//static void hexdump_window(uintptr_t addr, const uint8_t *p, size_t prefix);
/* ===== Safe, varargs-free hex helpers ===== */
static inline char hexdig(unsigned v) {
    static const char H[16] = "0123456789abcdef";
    return H[v & 0xF];
}

static void print_hex64(uint64_t x) {
    char b[16];
    for (int i = 15; i >= 0; --i) { b[i] = hexdig(x); x >>= 4; }
    serial_puts(b);
}

static void print_hex8(uint8_t v) {
    serial_putc(hexdig(v >> 4));
    serial_putc(hexdig(v));
}

/* --------------------------------------------------------------------------------
 * Minimal JSON helpers (optional; kept as static so they can be optimized out)
 * -------------------------------------------------------------------------------- */
static int json_get_str(const char *json, const char *key, char *out, size_t osz) {
    if (!json || !key || !out || osz == 0) return -1;
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    while (*p && *p != '"') p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < osz - 1) out[i++] = *p++;
    if (*p != '"') return -1;
    out[i] = 0;
    return 0;
}

static int json_get_int(const char *json, const char *key, long *out) {
    if (!json || !key || !out) return -1;
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (!*p) return -1;
    char *end = NULL;
    long v = strtol(p, &end, 0);
    if (end == p) return -1;
    *out = v;
    return 0;
}

// Auto-detects agent type and spawns with default priority 0
int load_agent_auto(const void *buf, size_t len) {
    // AGENT_FORMAT_ELF is usually 0 in this codebase, but use a detection if needed
    return load_agent_with_prio(buf, len, AGENT_FORMAT_ELF, 0);
}

/* --------------------------------------------------------------------------------
 * Loader-local tiny low-memory arena fallback (used if kalloc fails)
 * -------------------------------------------------------------------------------- */
static uint8_t s_loader_arena[512 * 1024];  // 512 KiB
static size_t  s_loader_off = 0;

static void *kalloc_aligned_or_arena(size_t bytes, size_t align) {
    if (align == 0) align = PAGE_SIZE;
    // try heap
    size_t req = bytes + align + 16; // a little headroom
    void *p = kalloc(req);
    if (!p) {
        serial_printf("[loader] kalloc %zu failed, trying arena\n", (unsigned long)bytes);
        uintptr_t base = (uintptr_t)s_loader_arena;
        uintptr_t cur  = align_up_ptr(base + s_loader_off, align);
        uintptr_t end  = cur + bytes;
        if (end > base + sizeof(s_loader_arena)) {
            serial_printf("[loader] arena OOM (need %zu, have %zu)\n",
                          (unsigned long)bytes,
                          (unsigned long)(sizeof(s_loader_arena) - s_loader_off));
            return NULL;
        }
        s_loader_off = (size_t)(end - base);
        return (void *)cur;
    }
    uintptr_t up = (uintptr_t)p;
    up = align_up_ptr(up, align);
    return (void *)up;
}

/* --------------------------------------------------------------------------------
 * Relocations: handle R_X86_64_RELATIVE from .rela.dyn / .rela.plt
 * -------------------------------------------------------------------------------- */
static size_t apply_relocations_rela(uint8_t *load_base, uint64_t lo_for_exec,
                                     const Elf64_Ehdr *eh, const void *img, size_t sz)
{
    (void)sz;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const uint8_t *)img + eh->e_phoff);
    const Elf64_Phdr *dyn_ph = NULL;

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) { dyn_ph = &ph[i]; break; }
    }
    if (!dyn_ph) return 0;

    const Elf64_Dyn *dyn = (const Elf64_Dyn *)((const uint8_t *)img + dyn_ph->p_offset);
    size_t dyn_cnt = dyn_ph->p_filesz / sizeof(Elf64_Dyn);

    uint64_t rela = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
    uint64_t jmprel = 0, pltrel_sz = 0, pltrel_type = 0;

    for (size_t i = 0; i < dyn_cnt; ++i) {
        switch (dyn[i].d_tag) {
            case DT_RELA:     rela       = dyn[i].d_un.d_ptr; break;
            case DT_RELASZ:   rela_sz    = dyn[i].d_un.d_val; break;
            case DT_RELAENT:  rela_ent   = dyn[i].d_un.d_val; break;
            case DT_JMPREL:   jmprel     = dyn[i].d_un.d_ptr; break;
            case DT_PLTRELSZ: pltrel_sz  = dyn[i].d_un.d_val; break;
            case DT_PLTREL:   pltrel_type= dyn[i].d_un.d_val; break;
            default: break;
        }
    }

    size_t applied = 0;

    // Helper to process a single rela table
    // NOTE: table addresses are *virtual addrs* from the executable view.
    //       For ET_EXEC we map at load_base such that vaddr X becomes load_base + (X - lo_for_exec).
    //       So in-memory pointer = load_base + (rela_vaddr - lo_for_exec).
    if (rela && rela_sz) {
        if (rela_ent == 0) rela_ent = sizeof(Elf64_Rela);
        size_t n = rela_sz / rela_ent;
        const Elf64_Rela *rt = (const Elf64_Rela *)((uintptr_t)load_base + (rela - lo_for_exec));
        for (size_t i = 0; i < n; ++i) {
            uint32_t type = (uint32_t)ELF64_R_TYPE(rt[i].r_info);
            // only handle RELATIVE
            if (type == R_X86_64_RELATIVE) {
                uint8_t *where = (uint8_t *)((uintptr_t)load_base + (rt[i].r_offset - lo_for_exec));
                uintptr_t val = (uintptr_t)load_base + (uintptr_t)(rt[i].r_addend - lo_for_exec);
                *(uintptr_t *)where = val;
                applied++;
            }
        }
    }

    if (jmprel && pltrel_sz && (pltrel_type == DT_RELA)) {
        size_t n = pltrel_sz / sizeof(Elf64_Rela);
        const Elf64_Rela *rt = (const Elf64_Rela *)((uintptr_t)load_base + (jmprel - lo_for_exec));
        for (size_t i = 0; i < n; ++i) {
            uint32_t type = (uint32_t)ELF64_R_TYPE(rt[i].r_info);
            if (type == R_X86_64_RELATIVE) {
                uint8_t *where = (uint8_t *)((uintptr_t)load_base + (rt[i].r_offset - lo_for_exec));
                uintptr_t val = (uintptr_t)load_base + (uintptr_t)(rt[i].r_addend - lo_for_exec);
                *(uintptr_t *)where = val;
                applied++;
            }
        }
    }

    return applied;
}

/* --------------------------------------------------------------------------------
 * Hex dump helpers
 * -------------------------------------------------------------------------------- */
#if VERBOSE
/* addr is the address of p[0] in memory; prints n bytes */
static void dump_bytes(uintptr_t addr, const uint8_t *p, size_t n) {
    size_t i = 0;
    while (i < n) {
        /* Address prefix */
        serial_puts("[dump] 0x");
        print_hex64(addr + i);
        serial_puts(" : ");

        /* Hex bytes */
        size_t j = 0;
        for (; j < 16 && i + j < n; ++j) {
            print_hex8(p[i + j]);
            serial_putc(' ');
        }
        /* pad if short line */
        for (; j < 16; ++j) serial_puts("   ");

        /* ASCII */
        serial_puts(" |");
        j = 0;
        for (; j < 16 && i + j < n; ++j) {
            uint8_t c = p[i + j];
            serial_putc((c >= 32 && c < 127) ? c : '.');
        }
        serial_puts("|\r\n");
        i += 16;
    }
}

/* Centered 64-byte window around entry (clamped at start) */
static void hexdump_window(const void *entry) {
    const uint8_t *p = (const uint8_t *)entry;

    /* First 4 bytes */
    serial_puts("[loader] entry first bytes: ");
    for (int i = 0; i < 4; ++i) {
        print_hex8(p[i]);
        if (i != 3) serial_putc(' ');
    }
    serial_puts("\r\n");

    /* 64B window */
    uintptr_t ep = (uintptr_t)p;
    const uint8_t *start = (ep >= 0x20) ? (p - 0x20) : p;
    size_t head = (size_t)(p - start);
    size_t total = head + 0x40;             /* 32 before + 32 after */

    serial_puts("[loader] dumping 64B around entry 0x");
    print_hex64((uint64_t)ep);
    serial_puts("\r\n");

    dump_bytes((uintptr_t)start, start, total);
}

#else
static inline void dump_bytes(uintptr_t a, const uint8_t *p, size_t n) { (void)a; (void)p; (void)n; }
static inline void hexdump_window(const void *entry) { (void)entry; }
#endif

/* --------------------------------------------------------------------------------
 * ELF mapper + spawner
 * -------------------------------------------------------------------------------- */
static int elf_map_and_spawn(const void *img, size_t sz, const char *path, int prio) {
    if (!img || sz < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)img;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64) return -1;
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return -1;
    if (eh->e_phoff == 0 || eh->e_phnum == 0) return -1;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const uint8_t *)img + eh->e_phoff);

    /* Find the span of PT_LOAD segments */
    uint64_t lo = UINT64_MAX, hi = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_memsz == 0) continue;
        lo = MIN(lo, ph[i].p_vaddr);
        hi = MAX(hi, ph[i].p_vaddr + ph[i].p_memsz);
    }
    if (lo == UINT64_MAX) return -1;

    uint64_t span = hi - lo;
    serial_printf("[loader] ELF: type=%u phnum=%u e_entry=0x%lx\n",
                  (unsigned)eh->e_type, (unsigned)eh->e_phnum, (unsigned long)eh->e_entry);
    serial_printf("[loader] span: lo=0x%lx hi=0x%lx span=%lu\n",
                  (unsigned long)lo, (unsigned long)hi, (unsigned long)span);

    /* Allocate a contiguous region */
    uint8_t *load_base = (uint8_t *)kalloc_aligned_or_arena((size_t)align_up_u64(span, PAGE_SIZE), PAGE_SIZE);
    if (!load_base) return -7; /* ENOMEM-ish */

    /* Map PT_LOAD segments */
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t dst_off = ph[i].p_vaddr - lo;
        uint8_t *dst = load_base + dst_off;
        size_t fsz = (size_t)ph[i].p_filesz;
        size_t msz = (size_t)ph[i].p_memsz;

        if (ph[i].p_offset + fsz > sz) return -1;

        memset(dst, 0, msz);
        if (fsz) memcpy(dst, (const uint8_t *)img + ph[i].p_offset, fsz);

        serial_printf("[loader] PT_LOAD: vaddr=0x%lx file=%lu mem=%lu fl=0x%lx -> [%p..%p)\n",
                      (unsigned long)ph[i].p_vaddr,
                      (unsigned long)fsz, (unsigned long)msz,
                      (unsigned long)ph[i].p_flags,
                      dst, dst + msz);
    }

    /* Apply RELA (RELATIVE only) */
    size_t applied = apply_relocations_rela(load_base, lo, eh, img, sz);
    serial_printf("[loader] relocations: applied %lu R_X86_64_RELATIVE\n", (unsigned long)applied);

    /* Compute runtime entry */
    uintptr_t runtime_entry =
        (eh->e_type == ET_EXEC)
            ? (uintptr_t)load_base + (uintptr_t)(eh->e_entry - lo)
            : (uintptr_t)load_base + (uintptr_t)eh->e_entry;

    /* Print runtime entry without width/length specifiers */
    serial_puts("[loader] runtime entry (");
    serial_puts((eh->e_type == ET_EXEC) ? "ET_EXEC" : "ET_DYN");
    serial_puts(") = 0x");
    print_hex64((uint64_t)runtime_entry);
    serial_puts("\r\n");

    /* Peek around entry safely (no varargs) */
    const uint8_t *entry_ptr = (const uint8_t *)runtime_entry;
    serial_puts("[loader] entry first bytes: ");
    for (int i = 0; i < 4; ++i) {
        print_hex8(entry_ptr[i]);
        if (i != 3) serial_putc(' ');
    }
    serial_puts("\r\n");

    serial_puts("[loader] dumping 64B around entry 0x");
    //print_hex64((uint64_t)runtime_entry);
    serial_puts("\r\n");
    //hexdump_window((const void *)entry_ptr);

    /* Notify RegX gate (if installed) */
    agent_gate_fn gate = agent_loader_get_gate();
    if (gate) {
        char name[32] = {0};
        if (path && *path) {
            const char *b = path, *p = path;
            while (*p) { if (*p == '/' || *p == '\\') b = p + 1; ++p; }
            size_t i = 0;
            while (b[i] && b[i] != '.' && i < sizeof(name) - 1) { name[i] = b[i]; ++i; }
            if (name[0] == '\0') { name[0] = '('; name[1] = 'e'; name[2] = 'l'; name[3] = 'f'; name[4] = ')'; }
        } else {
            name[0] = '('; name[1] = 'e'; name[2] = 'l'; name[3] = 'f'; name[4] = ')';
        }

        char entry_hex[2 + 16 + 1]; /* "0x" + 16 nibbles + NUL */
        entry_hex[0] = '0'; entry_hex[1] = 'x';
        uint64_t v = (uint64_t)runtime_entry;
        for (int i = 0; i < 16; ++i) {
            entry_hex[2 + 15 - i] = hexdig((unsigned)(v & 0xF));
            v >>= 4;
        }
        entry_hex[18] = '\0';

        const char *entry_sym = "agent_main";
        const char *caps = "";
        serial_puts("[loader] gate: name=\"");
        serial_puts(name);
        serial_puts("\" entry=\"");
        serial_puts(entry_sym);
        serial_puts("\" @ ");
        serial_puts(entry_hex);
        serial_puts(" caps=\"\" path=\"");
        serial_puts(path ? path : "(null)");
        serial_puts("\"\r\n");

        gate(name, entry_sym, entry_hex, caps, path ? path : "(null)");
    }

    /* Bridge to thread spawn (set by agent_loader_pub.c) */
    extern int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);
    int rc = -38; /* default if bridge missing */
    if (__agent_loader_spawn_fn) {
        rc = __agent_loader_spawn_fn(path ? path : "(elf)", (void *)runtime_entry, prio);
        serial_printf("[loader] register_and_spawn rc=%d\n", rc);
    }
    return rc;
}


/* --------------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------------- */
int load_agent_with_prio(const void *image, size_t size, agent_format_t fmt, int prio) {
    if (!image || size == 0) return -1;
    switch (fmt) {
        case AGENT_FORMAT_ELF:
            return elf_map_and_spawn(image, size, NULL, prio);
        default:
            return -38; // ENOSYS
    }
}

int load_agent(const void *image, size_t size, agent_format_t fmt) {
    return load_agent_with_prio(image, size, fmt, /*prio*/200);
}

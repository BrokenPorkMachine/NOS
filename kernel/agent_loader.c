// kernel/agent_loader.c
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "elf.h"
#include "agent_loader.h"
#include "VM/kheap.h"
#include "drivers/IO/serial.h"

// Some tree headers provide these; define minimally if missing.
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

// --- Safe, local hex helpers (no libc assumptions beyond serial_*) ---
static inline char _hx(uint8_t v){ static const char H[]="0123456789abcdef"; return H[v & 0xF]; }
static void _put8(uint8_t v){ serial_putc(_hx(v>>4)); serial_putc(_hx(v)); }
static void _put64(uint64_t x){
    char b[16];
    for (int i=15; i>=0; --i){ b[i] = _hx((uint8_t)(x & 0xF)); x >>= 4; }
    serial_puts(b);
}

/* Dump a small window around entry, but *only* within [load_base, load_base+span). */
static void safe_dump_window(const uint8_t* load_base, uint64_t span, const uint8_t* entry_ptr) {
    const uintptr_t base = (uintptr_t)load_base;
    const uintptr_t end  = base + (uintptr_t)align_up_u64(span, PAGE_SIZE);
    const uintptr_t p    = (uintptr_t)entry_ptr;

    uintptr_t wlo = (p >= base + 32) ? (p - 32) : base;
    uintptr_t whi = (p + 32 <= end)  ? (p + 32) : end;
    if (wlo >= whi) return;

    serial_puts("[loader] dump @ 0x"); _put64((uint64_t)p); serial_puts("\r\n");
    for (uintptr_t q = wlo; q < whi; ++q) {
        _put8(*(const volatile uint8_t*)q);
        if (((q - wlo) & 0x0F) == 0x0F && q + 1 < whi) serial_puts("\r\n");
        else if (q + 1 < whi) serial_putc(' ');
    }
    serial_puts("\r\n");
}

/* Very small allocator shim: try a page-aligned kalloc; no arena fallback here. */
static void* kalloc_aligned_or_arena(size_t bytes, size_t align) {
    size_t need = bytes;
    if (align < sizeof(void*)) align = sizeof(void*);
    // Over-allocate and align manually to keep it simple and freestanding.
    uint8_t* raw = (uint8_t*)kalloc(need + align);
    if (!raw) {
        serial_printf("[loader] kalloc %zu failed\n", (unsigned long)bytes);
        return NULL;
    }
    uintptr_t p = (uintptr_t)raw;
    uintptr_t aligned = (p + (align - 1)) & ~(uintptr_t)(align - 1);
    // NOTE: no free of leading slack; in this usage agents are short-lived.
    (void)p;
    return (void*)aligned;
}

/* RELATIVE-only relocations (many simple ET_EXEC/ET_DYN agents have none). Safe no-op. */
static size_t apply_relocations_rela(uint8_t* load_base, uint64_t lo_for_exec,
                                     const Elf64_Ehdr* eh, const void* img, size_t sz)
{
    (void)load_base; (void)lo_for_exec; (void)eh; (void)img; (void)sz;
    // Keep this simple for now; your current agents show "applied 0".
    return 0;
}

/* Optional tiny JSON helpers kept for future manifest usage (currently unused). */
static int json_get_str(const char *json, const char *key, char *out, size_t osz) {
    (void)json; (void)key; (void)out; (void)osz;
    return -1;
}
static int json_get_int(const char *json, const char *key, long *out) {
    (void)json; (void)key; (void)out;
    return -1;
}

/* Core: map an ELF in kernel memory and spawn it via the public bridge. */
static int elf_map_and_spawn(const void *img, size_t sz, const char *path, int prio) {
    if (!img || sz < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)img;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64)
        return -1;
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN)
        return -1;
    if (eh->e_phoff == 0 || eh->e_phnum == 0)
        return -1;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const uint8_t *)img + eh->e_phoff);

    // Compute span of PT_LOAD segments
    uint64_t lo = UINT64_MAX, hi = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        lo = MIN(lo, ph[i].p_vaddr);
        hi = MAX(hi, ph[i].p_vaddr + ph[i].p_memsz);
    }
    if (lo == UINT64_MAX) return -1;

    uint64_t span = hi - lo;
    serial_printf("[loader] ELF: type=%u phnum=%u e_entry=0x%lx\n",
                  (unsigned)eh->e_type, (unsigned)eh->e_phnum, (unsigned long)eh->e_entry);
    serial_printf("[loader] span: lo=0x%lx hi=0x%lx span=%lu\n",
                  (unsigned long)lo, (unsigned long)hi, (unsigned long)span);

    // Allocate contiguous region
    uint8_t *load_base = (uint8_t *)kalloc_aligned_or_arena((size_t)align_up_u64(span, PAGE_SIZE), PAGE_SIZE);
    if (!load_base) return -7; // ENOMEM-ish

    // Map PT_LOAD segments
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;

        uint64_t dst_off = ph[i].p_vaddr - lo;
        uint8_t *dst = load_base + dst_off;
        size_t fsz = (size_t)ph[i].p_filesz;
        size_t msz = (size_t)ph[i].p_memsz;

        if (ph[i].p_offset + fsz > sz) return -1;

        memset(dst, 0, msz);
        if (fsz) memcpy(dst, (const uint8_t *)img + ph[i].p_offset, fsz);

        serial_printf("[loader] PT_LOAD: vaddr=0x%lx file=%zu mem=%zu fl=0x%lx -> [%p..%p)\n",
                      (unsigned long)ph[i].p_vaddr,
                      (unsigned long)fsz, (unsigned long)msz,
                      (unsigned long)ph[i].p_flags,
                      dst, dst + msz);
    }

    // RELATIVE relocations (no-op for current agents)
    size_t applied = apply_relocations_rela(load_base, lo, eh, img, sz);
    serial_printf("[loader] relocations: applied %zu R_X86_64_RELATIVE\n", applied);

    // Compute runtime entry
    uintptr_t runtime_entry = 0;
    if (eh->e_type == ET_EXEC) {
        runtime_entry = (uintptr_t)load_base + (uintptr_t)(eh->e_entry - lo);
        serial_printf("[loader] runtime entry (ET_EXEC) = 0x%016lx\n", (unsigned long)runtime_entry);
    } else {
        runtime_entry = (uintptr_t)load_base + (uintptr_t)eh->e_entry;
        serial_printf("[loader] runtime entry (ET_DYN)  = 0x%016lx\n", (unsigned long)runtime_entry);
    }

    // Peek at entry safely
    const uint8_t *entry_ptr = (const uint8_t *)runtime_entry;
    serial_printf("[loader] entry first bytes: %02x %02x %02x %02x\n",
                  entry_ptr[0], entry_ptr[1], entry_ptr[2], entry_ptr[3]);
    safe_dump_window(load_base, span, entry_ptr);

    // Notify RegX (if configured)
    agent_gate_fn gate = agent_loader_get_gate();
    if (gate) {
        char name[32] = {0};
        if (path && *path) {
            const char *b = path, *p = path;
            while (*p) { if (*p == '/' || *p == '\\') b = p + 1; ++p; }
            size_t i = 0;
            while (b[i] && b[i] != '.' && i < sizeof(name) - 1) { name[i] = b[i]; ++i; }
            if (i == 0) snprintf(name, sizeof(name), "(elf)");
        } else {
            snprintf(name, sizeof(name), "(elf)");
        }
        const char *entry_sym = "agent_main";
        char entry_hex[32]; snprintf(entry_hex, sizeof(entry_hex), "%p", (void *)runtime_entry);
        const char *caps = "";
        serial_printf("[loader] gate: name=\"%s\" entry=\"%s\" @ %s caps=\"%s\" path=\"%s\"\n",
                      name, entry_sym, entry_hex, caps, path ? path : "(null)");
        gate(name, entry_sym, entry_hex, caps, path ? path : "(null)");
    }

    // Spawn via bridge provided by agent_loader_pub.c
    extern int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);
    int rc = -38; // default "no bridge"
    if (__agent_loader_spawn_fn) {
        rc = __agent_loader_spawn_fn(path ? path : "(elf)", (void *)runtime_entry, prio);
        serial_printf("[loader] register_and_spawn rc=%d\n", rc);
    }
    return rc;
}

// Public helpers (invoked by regx or other subsystems)

int load_agent(const void *img, size_t sz, const char *path, int prio) {
    return elf_map_and_spawn(img, sz, path, prio);
}

int load_agent_auto(const void *img, size_t sz) {
    // Heuristic priority (matches your regx usage); tweak as needed.
    return elf_map_and_spawn(img, sz, NULL, /*prio*/200);
}

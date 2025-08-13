#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"

// Ensure the raw binary begins with a jump to `_start` so nboot can call the
// start of the buffer directly.
__asm__(".global o2_entry_stub\n o2_entry_stub: jmp _start\n");

// ========== Settings ==========

#define STAGE1_MODULE_NAME "n2.bin"

#ifndef VERBOSE
#define VERBOSE 1
#endif

#if VERBOSE
#define vprint(s) serial_print(s)
#define vhex(v)   print_hex(v)
#else
#define vprint(s) (void)0
#define vhex(v)   (void)0
#endif

// Forward declarations so `_start` can appear before helper routines. Placing
// the entrypoint first ensures the raw binary begins with `_start`, allowing
// nboot to jump directly to the start of `O2.bin` without needing an offset.
static void serial_init(void);
static void serial_print(const char *s);
static void print_hex(uint64_t v);
static int  streq(const char *a, const char *b);
static int  load_elf64(const void *image, size_t size, void **entry,
                       kernel_segment_t *segs, uint32_t *segc);

// ========== Main Stage0 Entrypoint ==========

void _start(bootinfo_t *bi) {
    serial_init();
    vprint("\r\n[O2] stage0 handoff from nboot\r\n");
    if (!bi) { vprint("[O2] stage0 ERROR: no bootinfo\r\n"); while(1){} }

    // Find n2.bin module
    void *n2_img = 0;
    size_t n2_size = 0;
    for (uint32_t i = 0; i < bi->module_count; i++) {
        // Convert bootinfo module name (char*) to C string
        const char *mname = bi->modules[i].name;
        if (streq(mname, STAGE1_MODULE_NAME)) {
            n2_img = bi->modules[i].base;
            n2_size = bi->modules[i].size;
            vprint("[O2] stage0 found n2.bin @");
            vhex((uint64_t)(uintptr_t)n2_img);
            vprint(" sz="); vhex(n2_size); vprint("\r\n");
            break;
        }
    }
    if (!n2_img) {
        vprint("[O2] stage0 ERROR: no n2.bin found in modules\r\n");
        while(1){}
    }

    // Load ELF64 n2.bin into memory
    kernel_segment_t n2_segments[MAX_KERNEL_SEGMENTS];
    uint32_t n2_segment_count = 0;
    void (*n2_entry)(bootinfo_t *) = 0;
    if (load_elf64(n2_img, n2_size, (void**)&n2_entry, n2_segments, &n2_segment_count) < 0) {
        vprint("[O2] stage0 ERROR: ELF64 load failed\r\n");
        while(1){}
    }

    // Optionally update bootinfo with n2 segment info (optional)
    // memcpy(bi->kernel_segments, n2_segments, sizeof(kernel_segment_t)*n2_segment_count);
    // bi->kernel_segment_count = n2_segment_count;

    vprint("[O2] stage0 Jumping to stage1 (n2)...\r\n");
    n2_entry(bi);

    while(1){}
}

// ========== Minimal stdlib ==========

static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int streq(const char *a, const char *b) { while(*a && *b && *a==*b) a++,b++; return *a==*b; }
static void itoahex(uint64_t v, char *buf) {
    static const char *hex = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i)
        buf[2 + i] = hex[(v >> ((15 - i) * 4)) & 0xF];
    buf[18] = '\0';
}

// ========== Serial output (optional, for qemu/bochs debugging) ==========
// Initialize COM1 so early boot logs appear even if firmware hasn't set it up.
#define COM1_PORT 0x3F8
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0,%1" : : "a"(val), "Nd"(port)); }
static void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable all interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB
    outb(COM1_PORT + 0, 0x03); // Set divisor to 3 (38400 baud)
    outb(COM1_PORT + 1, 0x00); // High byte of divisor
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}
static void serial_putc(char c) { outb(COM1_PORT, c); }
static void serial_print(const char *s) { while (*s) serial_putc(*s++); }
static void print_hex(uint64_t v) { char b[20]; itoahex(v, b); serial_print(b); }

// Helpers to read little-endian values into 64-bit integers without sign extension
static uint64_t rd16(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8);
}
static uint64_t rd32(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

// ========== ELF64 Loader ==========

static int load_elf64(const void *image, size_t size, void **entry,
                      kernel_segment_t *segs, uint32_t *segc) {
    const uint8_t *d = (const uint8_t *)image;
    if (size < 64) return -1;
    if (!(d[0]==0x7F && d[1]=='E' && d[2]=='L' && d[3]=='F')) return -2;

    uint64_t e_entry     = rd64(d + 24);
    uint64_t e_phoff     = rd64(d + 32);
    uint64_t e_phentsize = rd16(d + 54);
    uint64_t e_phnum     = rd16(d + 56);

    const uint8_t *ph = d + e_phoff;
    uint32_t count = 0;
    for (uint64_t i = 0; i < e_phnum; ++i, ph += e_phentsize) {
        uint64_t p_type   = rd32(ph + 0);
        uint64_t p_flags  = rd32(ph + 4);
        uint64_t p_offset = rd64(ph + 8);
        uint64_t p_vaddr  = rd64(ph + 16);
        uint64_t p_paddr  = rd64(ph + 24);
        uint64_t p_filesz = rd64(ph + 32);
        uint64_t p_memsz  = rd64(ph + 40);
        // uint64_t p_align  = rd64(ph + 48); // currently unused

        if (p_type != 1) continue; // PT_LOAD

        // Physical addresses of some segments may be either zero or lie
        // above the 4 GB mark which our early loader cannot directly
        // address.  In those cases fall back to the virtual address so the
        // segment is copied to a sensible location.
        uintptr_t dst = (uintptr_t)p_paddr;
        if (dst == 0 || dst >= 0x100000000ULL) {
            dst = (uintptr_t)p_vaddr;
        }

        memcpy((void *)dst, d + p_offset, (size_t)p_filesz);
        if (p_memsz > p_filesz)
            memset((void *)(dst + p_filesz), 0, (size_t)(p_memsz - p_filesz));

        // Print info over serial
        vprint("[O2] n2.seg: vaddr="); vhex(p_vaddr);
        vprint(" paddr="); vhex(p_paddr);
        vprint(" filesz="); vhex(p_filesz);
        vprint(" memsz="); vhex(p_memsz);
        vprint(" flags="); vhex(p_flags); vprint("\r\n");

        if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
            segs[count].vaddr = p_vaddr;
            segs[count].paddr = p_paddr;
            segs[count].filesz = p_filesz;
            segs[count].memsz = p_memsz;
            segs[count].flags = (uint32_t)p_flags;
            segs[count].name[0]=0;
            ++count;
        }
    }
    if (segc) *segc = count;
    *entry = (void *)(uintptr_t)e_entry;
    return 0;
}


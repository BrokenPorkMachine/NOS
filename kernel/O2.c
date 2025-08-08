#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"

// Ensure the raw binary begins with a jump to `_start` so nboot can call the
// start of the buffer directly.
__asm__(".global o2_entry_stub\n o2_entry_stub: jmp _start\n");

// ========== Settings ==========

#define STAGE1_MODULE_NAME "n2.bin"

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
    serial_print("\r\n[O2/stage0] handoff from nboot\r\n");
    if (!bi) { serial_print("[O2/stage0] ERROR: no bootinfo\r\n"); while(1){} }

    // Find n2.bin module
    void *n2_img = 0;
    size_t n2_size = 0;
    for (uint32_t i = 0; i < bi->module_count; i++) {
        // Convert bootinfo module name (char*) to C string
        const char *mname = bi->modules[i].name;
        if (streq(mname, STAGE1_MODULE_NAME)) {
            n2_img = bi->modules[i].base;
            n2_size = bi->modules[i].size;
            serial_print("[O2/stage0] found n2.bin @");
            print_hex((uint64_t)(uintptr_t)n2_img);
            serial_print(" sz="); print_hex(n2_size); serial_print("\r\n");
            break;
        }
    }
    if (!n2_img) {
        serial_print("[O2/stage0] ERROR: no n2.bin found in modules\r\n");
        while(1){}
    }

    // Load ELF64 n2.bin into memory
    kernel_segment_t n2_segments[MAX_KERNEL_SEGMENTS];
    uint32_t n2_segment_count = 0;
    void (*n2_entry)(bootinfo_t *) = 0;
    if (load_elf64(n2_img, n2_size, (void**)&n2_entry, n2_segments, &n2_segment_count) < 0) {
        serial_print("[O2/stage0] ERROR: ELF64 load failed\r\n");
        while(1){}
    }

    // Optionally update bootinfo with n2 segment info (optional)
    // memcpy(bi->kernel_segments, n2_segments, sizeof(kernel_segment_t)*n2_segment_count);
    // bi->kernel_segment_count = n2_segment_count;

    serial_print("[O2/stage0] Jumping to stage1 (n2)...\r\n");
    n2_entry(bi);

    while(1){}
}

// ========== Minimal stdlib ==========

static void *memcpy(void *dst, const void *src, size_t n) { uint8_t *d=dst; const uint8_t *s=src; while (n--) *d++ = *s++; return dst; }
static void *memset(void *dst, int c, size_t n) { uint8_t *d=dst; while (n--) *d++ = (uint8_t)c; return dst; }
static int streq(const char *a, const char *b) { while(*a && *b && *a==*b) a++,b++; return *a==*b; }
static void itoahex(uint64_t v, char *buf) {
    for (int i=15; i>=0; --i) { buf[2+i] = "0123456789ABCDEF"[(v>>(i*4))&0xF]; }
    buf[0]='0'; buf[1]='x'; buf[18]=0;
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

// ========== ELF64 Loader ==========

static int load_elf64(const void *image, size_t size, void **entry,
                      kernel_segment_t *segs, uint32_t *segc) {
    typedef struct { unsigned char e_ident[16]; uint16_t e_type,e_machine; uint32_t e_version;
        uint64_t e_entry,e_phoff,e_shoff; uint32_t e_flags; uint16_t e_ehsize,e_phentsize,e_phnum;
        uint16_t e_shentsize,e_shnum,e_shstrndx; } Elf64_Ehdr;
    typedef struct { uint32_t p_type,p_flags; uint64_t p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_align; } Elf64_Phdr;

    if (size < sizeof(Elf64_Ehdr)) return -1;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (!(eh->e_ident[0]==0x7F && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return -2;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const uint8_t *)image + eh->e_phoff);
    uint32_t count = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i, ++ph) {
        if (ph->p_type != 1) continue; // PT_LOAD
        memcpy((void *)(uintptr_t)ph->p_paddr, (const uint8_t *)image + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void *)(uintptr_t)(ph->p_paddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);

        // Print info over serial
        serial_print("[O2] n2.seg: vaddr="); print_hex(ph->p_vaddr);
        serial_print(" paddr="); print_hex(ph->p_paddr);
        serial_print(" filesz="); print_hex(ph->p_filesz);
        serial_print(" memsz="); print_hex(ph->p_memsz);
        serial_print(" flags="); print_hex(ph->p_flags); serial_print("\r\n");

        if (segs && segc && count < MAX_KERNEL_SEGMENTS) {
            segs[count].vaddr = ph->p_vaddr;
            segs[count].paddr = ph->p_paddr;
            segs[count].filesz = ph->p_filesz;
            segs[count].memsz = ph->p_memsz;
            segs[count].flags = ph->p_flags;
            segs[count].name[0]=0;
            ++count;
        }
    }
    if (segc) *segc = count;
    *entry = (void *)(uintptr_t)eh->e_entry;
    return 0;
}

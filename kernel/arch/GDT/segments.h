#pragma once

/* ---------------- GDT selectors (byte values) ---------------- */
#define GDT_SEL_KERNEL_CODE  0x08
#define GDT_SEL_KERNEL_DATA  0x10
#define GDT_SEL_USER_CODE    0x1B
#define GDT_SEL_USER_DATA    0x23

/* RPL-tagged convenience forms (same value, explicit for legacy call sites) */
#define GDT_SEL_USER_CODE_R3  GDT_SEL_USER_CODE
#define GDT_SEL_USER_DATA_R3  GDT_SEL_USER_DATA

/* Optional: 64-bit TSS selector (system descriptor, 16 bytes total) */
#define GDT_SEL_TSS           0x48

/* Handy aliases used elsewhere */
#define KERNEL_CS GDT_SEL_KERNEL_CODE
#define KERNEL_DS GDT_SEL_KERNEL_DATA
#define USER_CS   GDT_SEL_USER_CODE_R3
#define USER_DS   GDT_SEL_USER_DATA_R3

/* ---------------- Access / Granularity presets ----------------
   Access byte: P | DPL | S | Type
   Granularity: lim[19:16] | AVL | L | D/B | G
   These match AMD64 long mode rules (code: L=1, D/B=0). */
#define ACC_P                0x80
#define ACC_DPL(n)           (((n) & 0x3) << 5)
#define ACC_S_CODEDATA       0x10
#define ACC_TYPE_CODE        0x08  /* executable */
#define ACC_TYPE_DATA        0x00  /* data */
#define ACC_TYPE_RW          0x02  /* readable code / writable data */

#define ACC_CODE64_DPL0 (ACC_P | ACC_DPL(0) | ACC_S_CODEDATA | ACC_TYPE_CODE | ACC_TYPE_RW) /* 0x9A */
#define ACC_DATA_DPL0   (ACC_P | ACC_DPL(0) | ACC_S_CODEDATA | ACC_TYPE_DATA | ACC_TYPE_RW) /* 0x92 */
#define ACC_CODE64_DPL3 (ACC_P | ACC_DPL(3) | ACC_S_CODEDATA | ACC_TYPE_CODE | ACC_TYPE_RW) /* 0xFA */
#define ACC_DATA_DPL3   (ACC_P | ACC_DPL(3) | ACC_S_CODEDATA | ACC_TYPE_DATA | ACC_TYPE_RW) /* 0xF2 */

/* TSS access (system descriptor, S=0) */
#define ACC_TSS_AVAIL    (ACC_P | 0x09)  /* available 64-bit TSS */
#define ACC_TSS_BUSY     (ACC_P | 0x0B)  /* busy 64-bit TSS */

/* Granularity bits */
#define GRAN_G           0x80  /* 4KiB granularity */
#define GRAN_DB          0x40  /* must be 0 for 64-bit code */
#define GRAN_L           0x20  /* 64-bit code segment */
#define GRAN_AVL         0x10

/* Presets */
#define GRAN_CODE64      (GRAN_G | GRAN_L) /* code: L=1, DB=0, G=1 */
#define GRAN_DATA        (GRAN_G)          /* data: L=0, DB ignored in long mode */

/* Index helper (entry number in the GDT) */
#define GDT_IDX(sel)     ((unsigned)((sel) >> 3))

/* Optional compile-time sanity checks (GCC/Clang) */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused)) static inline void __segments_sanity(void) {
    /* Kernel selectors must be ring 0; user must be ring 3 */
    _Static_assert((KERNEL_CS & 3u) == 0, "KERNEL_CS must have DPL=0");
    _Static_assert((KERNEL_DS & 3u) == 0, "KERNEL_DS must have DPL=0");
    _Static_assert((USER_CS   & 3u) == 3, "USER_CS must have DPL=3");
    _Static_assert((USER_DS   & 3u) == 3, "USER_DS must have DPL=3");
}
#endif

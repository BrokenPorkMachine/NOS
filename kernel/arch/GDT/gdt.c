#include "gdt.h"
#include "segments.h"
#include <stdint.h>
#include <string.h>

/* ----- GDTR ----- */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Your 8-byte legacy descriptor (code/data) */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;       /* type | S | DPL | P */
    uint8_t  granularity;  /* limit[19:16] | AVL | L | D/B | G */
    uint8_t  base_high;
} __attribute__((packed));

/* 16-byte TSS/ system descriptor (only if you use TSS) */
struct gdt_tss_desc {
    uint16_t limit0;
    uint16_t base0;
    uint8_t  base1;
    uint8_t  access;      /* type=0x9/0xB, P=1, DPL=0, S=0 */
    uint8_t  gran;        /* limit[19:16] | AVL | 0 | 0 | G */
    uint8_t  base2;
    uint32_t base3;
    uint32_t reserved;
} __attribute__((packed));

/* Access bits */
#define ACC_P        0x80
#define ACC_DPL(n)   (((n) & 0x3) << 5)
#define ACC_S_CODE   0x10     /* descriptor type: 1 for code/data */
#define ACC_TYPE_C   0x08     /* executable (code) */
#define ACC_TYPE_RW  0x02     /* readable (code) / writable (data) */
#define ACC_CODE64   (ACC_P | ACC_DPL(0) | ACC_S_CODE | ACC_TYPE_C | ACC_TYPE_RW)  /* 0x9A */
#define ACC_DATA     (ACC_P | ACC_DPL(0) | ACC_S_CODE | ACC_TYPE_RW)               /* 0x92 */

/* Gate/system types for TSS */
#define ACC_TSS_AVAIL  (ACC_P | /*DPL0*/ 0 | /*S=0*/ 0 | 0x09) /* 64-bit available TSS */
#define ACC_TSS_BUSY   (ACC_P | 0x0B)

/* Granularity bits */
#define GRAN_G       0x80     /* 4KiB granularity */
#define GRAN_DB      0x40     /* D/B: must be 0 for 64-bit code; ignored for data in long mode */
#define GRAN_L       0x20     /* 64-bit code segment */
#define GRAN_AVL     0x10     /* available for software */

/* Helpers */
#define SEL2IDX(sel) ((unsigned)((sel) >> 3))

/* ---- Table size:
 * Keep some slack for TSS (2 slots) if defined.
 */
#ifndef GDT_SLOTS
# define GDT_SLOTS  11
#endif

static struct gdt_entry gdt[GDT_SLOTS];   /* 8-byte entries */
static struct gdt_ptr gp;

extern void gdt_flush(const void *gdtr);
extern void gdt_flush_with_tr(const void *gdtr, uint16_t tss_sel); /* provided in asm I shared */

/* ------------------------------------------------------------------ */
/* 8-byte code/data descriptor writer                                  */
/* ------------------------------------------------------------------ */
static void gdt_set_gate(unsigned idx, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran)
{
    if (idx >= GDT_SLOTS) return;

    gdt[idx].base_low     = (uint16_t)(base & 0xFFFFu);
    gdt[idx].base_middle  = (uint8_t)((base >> 16) & 0xFFu);
    gdt[idx].base_high    = (uint8_t)((base >> 24) & 0xFFu);

    gdt[idx].limit_low    = (uint16_t)(limit & 0xFFFFu);
    gdt[idx].granularity  = (uint8_t)(((limit >> 16) & 0x0Fu) | (gran & 0xF0u));

    gdt[idx].access       = access;
}

/* ------------------------------------------------------------------ */
/* 16-byte TSS descriptor writer (occupies one 16-byte slot)           */
/* Writes into two 8-byte entries worth of space starting at idx.      */
/* ------------------------------------------------------------------ */
static void gdt_set_tss_desc(unsigned idx, uint64_t base, uint32_t limit, int busy)
{
#if defined(GDT_SEL_TSS)
    if (idx + 1 >= GDT_SLOTS) return;

    /* We alias the two entries as a 16-byte system descriptor region */
    struct gdt_tss_desc *td = (struct gdt_tss_desc *)&gdt[idx];

    memset(td, 0, sizeof(*td));
    td->limit0 = (uint16_t)(limit & 0xFFFFu);
    td->base0  = (uint16_t)(base & 0xFFFFu);
    td->base1  = (uint8_t)((base >> 16) & 0xFFu);
    td->access = busy ? ACC_TSS_BUSY : ACC_TSS_AVAIL;
    td->gran   = (uint8_t)(((limit >> 16) & 0x0Fu) | (GRAN_G));  /* G=1 typical; AVL/L/DB=0 */
    td->base2  = (uint8_t)((base >> 24) & 0xFFu);
    td->base3  = (uint32_t)((base >> 32) & 0xFFFFFFFFu);
    td->reserved = 0;
#else
    (void)idx; (void)base; (void)limit; (void)busy;
#endif
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

static void gdt_fill_core_segments(void)
{
    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Kernel ring 0: 64-bit code (L=1, D=0), data (G=1, DB=0, L=0) */
    gdt_set_gate(SEL2IDX(GDT_SEL_KERNEL_CODE), 0, 0xFFFFF, ACC_CODE64, GRAN_G | GRAN_L);
    gdt_set_gate(SEL2IDX(GDT_SEL_KERNEL_DATA), 0, 0xFFFFF, ACC_DATA,    GRAN_G /*| 0*/);

    /* Optional ring1/2 (if you defined them in segments.h) */
#ifdef GDT_SEL_RING1_CODE
    gdt_set_gate(SEL2IDX(GDT_SEL_RING1_CODE),  0, 0xFFFFF, (ACC_CODE64 | ACC_DPL(1)), GRAN_G | GRAN_L);
#endif
#ifdef GDT_SEL_RING1_DATA
    gdt_set_gate(SEL2IDX(GDT_SEL_RING1_DATA),  0, 0xFFFFF, (ACC_DATA   | ACC_DPL(1)), GRAN_G);
#endif
#ifdef GDT_SEL_RING2_CODE
    gdt_set_gate(SEL2IDX(GDT_SEL_RING2_CODE),  0, 0xFFFFF, (ACC_CODE64 | ACC_DPL(2)), GRAN_G | GRAN_L);
#endif
#ifdef GDT_SEL_RING2_DATA
    gdt_set_gate(SEL2IDX(GDT_SEL_RING2_DATA),  0, 0xFFFFF, (ACC_DATA   | ACC_DPL(2)), GRAN_G);
#endif

    /* User ring 3 */
    gdt_set_gate(SEL2IDX(GDT_SEL_USER_CODE),   0, 0xFFFFF, (ACC_CODE64 | ACC_DPL(3)), GRAN_G | GRAN_L);
    gdt_set_gate(SEL2IDX(GDT_SEL_USER_DATA),   0, 0xFFFFF, (ACC_DATA   | ACC_DPL(3)), GRAN_G);
}

void gdt_install(void)
{
    memset(gdt, 0, sizeof(gdt));
    gdt_fill_core_segments();

    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base  = (uint64_t)(uintptr_t)&gdt;

    /* Far reload CS + data segments */
    gdt_flush(&gp);
}

/* Optional: install TSS and load TR.
 * Requires: you have defined GDT_SEL_TSS in segments.h and provided a TSS struct.
 */
void gdt_install_with_tss(void *tss_base, uint32_t tss_limit)
{
#if defined(GDT_SEL_TSS)
    memset(gdt, 0, sizeof(gdt));
    gdt_fill_core_segments();

    /* TSS occupies a 16-byte descriptor; selector must point at idx with S=0 type */
    unsigned tss_idx = SEL2IDX(GDT_SEL_TSS);
    gdt_set_tss_desc(tss_idx, (uint64_t)(uintptr_t)tss_base, tss_limit, 0 /*available*/);

    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base  = (uint64_t)(uintptr_t)&gdt;

    /* Load GDT and TR in one go */
    gdt_flush_with_tr(&gp, GDT_SEL_TSS);
#else
    (void)tss_base; (void)tss_limit;
    /* Fall back to legacy install if no TSS selector is defined */
    gdt_install();
#endif
}

void gdt_get_entry(int n, struct gdt_entry *out)
{
    if (n >= 0 && n < (int)GDT_SLOTS && out) {
        *out = gdt[n];
    }
}

#pragma once

#include <stdint.h>
#include "segments.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Public GDT entry structure (legacy 8-byte code/data descriptors).
 * Exposed for diagnostics / unit tests (e.g., verifying L/D/G bits).
 * -------------------------------------------------------------------- */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;       /* type | S | DPL | P */
    uint8_t  granularity;  /* lim[19:16] | AVL | L | D/B | G */
    uint8_t  base_high;
} __attribute__((packed));

/* Total slots reserved in the table (code/data + optional TSS space). */
#ifndef GDT_SLOTS
#  define GDT_SLOTS 11
#endif

/* ----------------------------------------------------------------------
 * Install the core kernel/user segments and load the GDT.
 * Reloads CS/SS/DS/ES/FS/GS via a far return in the asm stub.
 * -------------------------------------------------------------------- */
void gdt_install(void);

/* ----------------------------------------------------------------------
 * (Optional) Install a 64-bit TSS descriptor and load TR.
 * Requires a selector like GDT_SEL_TSS defined in segments.h and a
 * valid TSS structure at tss_base with size = tss_limit + 1.
 * If GDT_SEL_TSS is not defined, this will fall back to gdt_install().
 * -------------------------------------------------------------------- */
void gdt_install_with_tss(void *tss_base, uint32_t tss_limit);

/* Copy out a raw 8-byte GDT entry for inspection/testing. */
void gdt_get_entry(int n, struct gdt_entry *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

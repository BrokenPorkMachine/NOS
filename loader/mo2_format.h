// ============================================================================
// loader/mo2_format.h  -- tiny MO2 container definitions
// ============================================================================
#ifndef MO2_FORMAT_H
#define MO2_FORMAT_H
#include <stdint.h>

#define MO2_MAGIC 0x324F4F4D /* 'MOO2' little-endian as 0x324F4F4D */
#define MO2_MAX_SECTS 16
#define MO2_MAX_NEEDS 8

/* File type */
#define MO2_FTYPE_EXEC   1
#define MO2_FTYPE_DYLIB  2
#define MO2_FTYPE_RELOC  3  /* .o2 */

/* CPU types (subset) */
#define MO2_CPU_X86_64   1
#define MO2_CPU_ARM64    2

/* Section kinds for relocation targeting */
#define MO2_SECT_TEXT    1
#define MO2_SECT_RODATA  2
#define MO2_SECT_DATA    3
#define MO2_SECT_BSS     4
#define MO2_SECT_PLT     5
#define MO2_SECT_GOT     6

/* Relocation types */
#define MO2_R_ABS64      1
#define MO2_R_REL32      2
#define MO2_R_GOT_LOAD   3
#define MO2_R_PLT_CALL   4

/* Symbol bindings */
#define MO2_SB_LOCAL  0
#define MO2_SB_GLOBAL 1
#define MO2_SB_WEAK   2

/* Protection flags (hint; runtime decides) */
#define MO2_PF_X 1
#define MO2_PF_W 2
#define MO2_PF_R 4

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;       // MO2_MAGIC
    uint16_t ftype;       // EXEC/DYLIB/RELOC
    uint16_t cpu;         // X86_64/ARM64
    uint32_t hdr_size;    // size of this header
    uint32_t nsects;      // number of sections
    uint32_t flags;       // reserved
    uint64_t entry_rva;   // entry RVA for EXEC, else 0
    uint32_t off_sections;// file offset to mo2_sect[]
    uint32_t off_dyn;     // file offset to mo2_dyninfo (if any), else 0
    uint32_t off_sym;     // file offset to mo2_sym[]
    uint32_t nsym;        // number of symbols
    uint32_t off_str;     // file offset to strtab bytes
    uint32_t str_sz;      // size of strtab
} mo2_hdr_t;

typedef struct {
    uint16_t kind;        // MO2_SECT_*
    uint16_t prot;        // PF bits
    uint32_t align;       // power-of-two alignment
    uint64_t vaddr;       // preferred VA (PIE uses RVA)
    uint64_t vsize;       // size in memory
    uint64_t foffset;     // file offset to data (0 for bss)
    uint64_t fsize;       // size in file
} mo2_sect_t;

typedef struct {
    uint32_t name_off;    // strtab offset
    uint32_t sect_id;     // MO2_SECT_* where symbol lives, 0 if undefined
    uint64_t value;       // RVA for defined; 0 if import
    uint16_t bind;        // LOCAL/GLOBAL/WEAK
    uint16_t ver;         // ABI version (1 default)
} mo2_sym_t;

typedef struct {
    uint32_t sect_id;     // relocation target section kind
    uint32_t r_offset;    // offset within that section
    uint16_t r_type;      // MO2_R_*
    uint16_t r_flags;     // bit0=weak, bit1=local, bit2=addend_inline
    uint32_t sym_idx;     // index into dyn symtab
    int64_t  addend;      // explicit addend (RELA-like)
} mo2_reloc_t;

typedef struct {
    uint32_t off_needed;  // strtab list: NUL-separated paths (@rpath, etc.)
    uint16_t n_needed;
    uint16_t eager;       // 0=lazy, 1=eager binding
    uint32_t off_imports; // mo2_sym_t indices (uint32_t array), length n_imports
    uint32_t n_imports;
    uint32_t off_reloc;   // mo2_reloc_t array
    uint32_t n_reloc;
    uint32_t off_init;    // RVA array (uint32_t)
    uint16_t n_init;
    uint16_t reserved;
    uint32_t off_fini;    // RVA array (uint32_t)
    uint16_t n_fini;
    uint16_t reserved2;
} mo2_dyninfo_t;
#pragma pack(pop)

#endif // MO2_FORMAT_H

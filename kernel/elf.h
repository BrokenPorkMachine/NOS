// kernel/elf.h — minimal ELF64 definitions for the loader (freestanding)
#ifndef NOS_ELF_H
#define NOS_ELF_H

#include <stdint.h>

#define EI_NIDENT   16
#define ELFMAG      "\177ELF"
#define SELFMAG     4
#define EI_CLASS    4
#define ELFCLASS64  2

/* e_type */
#define ET_NONE     0
#define ET_REL      1
#define ET_EXEC     2
#define ET_DYN      3

/* p_type */
#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2

/* dynamic tags */
#define DT_NULL     0
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9

/* x86_64 relocations */
#define R_X86_64_NONE      0
#define R_X86_64_RELATIVE  8

/* extractors for r_info */
#define ELF64_R_SYM(i)   ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i)  ((uint32_t)((i) & 0xffffffff))

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
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
    int64_t  d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} Elf64_Dyn;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

#endif /* NOS_ELF_H */

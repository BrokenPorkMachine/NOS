OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x100000;  /* Start at 1MB */

    /* Text section */
    __text_start = .;
    .text ALIGN(4K) : AT(ADDR(.text)) {
        *(.text*)
    }
    __text_end = .;

    /* Read-only data */
    __rodata_start = .;
    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        *(.rodata*)
    }
    __rodata_end = .;

    /* Data section */
    .data ALIGN(4K) : AT(ADDR(.data)) {
        *(.data*)
        *(.got*)
        *(.got.plt)
    }

    /* BSS section (no file space taken) */
    .bss ALIGN(4K) (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }
    _end = .;
}

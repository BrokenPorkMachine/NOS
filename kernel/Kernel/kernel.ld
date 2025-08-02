OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x100000;  /* Start at 1MB */

    /* Text section */
    .text ALIGN(4K) : AT(ADDR(.text)) {
        *(.text*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        *(.rodata*)
    }

    /* Data section */
    .data ALIGN(4K) : AT(ADDR(.data)) {
        *(.data*)
    }

    /* BSS section (no file space taken) */
    .bss ALIGN(4K) (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }
    _end = .;
}

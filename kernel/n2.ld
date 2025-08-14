OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    KERNEL_BASE_VIRT = 0x0;
    KERNEL_BASE_PHYS = 0x00100000;

    /* Link in the high-half (VMA), but load at physical (LMA) */
    . = KERNEL_BASE_VIRT + KERNEL_BASE_PHYS;

    /* Text section */
    __text_start = .;
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_BASE_VIRT) {
        *(.text*)
    }
    __text_end = .;

    /* Read-only data */
    __rodata_start = .;
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_BASE_VIRT) {
        *(.rodata*)
    }
    __rodata_end = .;

    /* Data section */
    __data_start = .;
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_BASE_VIRT) {
        *(.data*)
        *(.got*)
        *(.got.plt*)
    }
    __data_end = .;

    /* BSS section (no file space taken) */
    __bss_start = .;
    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_BASE_VIRT) {
        *(.bss*)
        *(COMMON)
    }
    __bss_end = .;

    _end = .;
}

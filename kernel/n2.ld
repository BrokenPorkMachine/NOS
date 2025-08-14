OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    KERNEL_BASE_VIRT = 0xFFFF800000000000;
    KERNEL_BASE_PHYS = 0x100000;
    . = KERNEL_BASE_VIRT + KERNEL_BASE_PHYS;  /* Link kernel in high-half */

    /* Text section */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_BASE_VIRT) {
        *(.text*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_BASE_VIRT) {
        *(.rodata*)
    }

    /* Data section */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_BASE_VIRT) {
        *(.data*)
        *(.got*)
        *(.got.plt)
    }

    /* BSS section (no file space taken) */
    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_BASE_VIRT) {
        *(.bss*)
        *(COMMON)
    }
    _end = .;
}

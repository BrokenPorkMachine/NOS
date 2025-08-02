OUTPUT_FORMAT("pei-x86-64")
ENTRY(efi_main)

SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text*)
    }

    .data : {
        *(.data*)
        *(.rodata*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }

    /DISCARD/ : { *(.note.gnu.*) *(.eh_frame*) }
}

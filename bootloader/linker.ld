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
        *(.bss*)
    }

    /DISCARD/ : { *(.note.gnu.*) *(.eh_frame*) }
}

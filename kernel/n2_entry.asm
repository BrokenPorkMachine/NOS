section .text
global _start

; Kernel entry symbol implemented in C
extern n2_main

_start:
    cld
    ; Bootloader passes bootinfo in RDI (SysV x86_64 ABI),
    ; so it's already in the correct register for n2_main.
    xor rbp, rbp
    call n2_main

.hang:
    cli
    hlt
    jmp .hang

; Indicate that this object file does not require an executable stack
section .note.GNU-stack noalloc nobits align=1

section .text
global _start

; Kernel entry symbol implemented in C
extern n2_main

_start:
    cld
    ; Bootloader already set up stack and passed bootinfo in rdi
    xor rbp, rbp
    call n2_main

.hang:
    cli
    hlt
    jmp .hang

; Indicate that this object file does not require an executable stack
section .note.GNU-stack noalloc nobits align=1

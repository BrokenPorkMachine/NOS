section .text
global _start

; Kernel entry symbol implemented in C
extern n2_main

_start:
    cld
    xor eax, eax
    lldt ax              ; disable any LDT
    ; Enable SSE/FXSR before any C code runs
    mov rax, cr0
    and eax, 0xFFFFFFFB  ; clear EM
    or  eax, 0x2         ; set MP
    mov cr0, rax
    mov rax, cr4
    or  eax, 0x600       ; set OSFXSR | OSXMMEXCPT
    mov cr4, rax
    ; Bootloader passes bootinfo in RDI (SysV x86_64 ABI)
    xor rbp, rbp
    call n2_main

.hang:
    cli
    hlt
    jmp .hang

; Indicate that this object file does not require an executable stack
section .note.GNU-stack noalloc nobits align=1

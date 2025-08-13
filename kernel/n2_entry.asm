%define BOOTSTRAP_STACK_SIZE (64 * 1024)

section .bss
align 16
global _kernel_stack_top
_kernel_stack_bottom:
    resb BOOTSTRAP_STACK_SIZE
_kernel_stack_top:

section .text
global _start

; Kernel entry symbol implemented in C
extern n2_main

_start:
    cli
    cld
    ; Set up a known-good stack before calling C code
    mov rsp, _kernel_stack_top

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

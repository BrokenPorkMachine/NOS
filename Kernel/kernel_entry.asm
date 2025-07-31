section .bss
align 16
kernel_stack:
    resb 16384            ; 16 KiB kernel stack
kernel_stack_top:

section .text
global _start

; Kernel entry symbol implemented in C
extern kernel_main

_start:
    cld

    ; rdi contains the bootinfo pointer from the bootloader
    mov rax, rdi          ; preserve bootinfo pointer

    ; Set up a small kernel stack
    lea rsp, [kernel_stack_top]
    xor rbp, rbp

    ; Pass bootinfo pointer to kernel_main in rdi
    mov rdi, rax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

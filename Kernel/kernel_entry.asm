section .text
global _start

; Optionally provide a pointer in rdi (for boot info, etc)
; If not used, set to 0

_start:
    ; -- Clear direction flag (some firmware may set it)
    cld

    ; -- Set up stack pointer (stack at 2MB, grows down)
    mov rsp, 0x200000
    xor rbp, rbp

    ; -- Optionally zero other callee-saved regs
    xor rsi, rsi
    xor rdi, rdi      ; rdi = NULL (unless boot info struct available)

    ; --- Debug: Output 'K' to port 0xE9 if you want early bochs/qemu debug
    ; mov al, 'K'
    ; out 0xE9, al

    ; -- Call C kernel main
    extern kernel_main
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

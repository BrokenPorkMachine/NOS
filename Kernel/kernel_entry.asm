section .text
global _start
_start:
    ; Set up stack pointer (at 2MB, grows down)
    mov rsp, 0x200000

    ; Call C entrypoint
    extern kernel_main
    call kernel_main

    ; Hang forever
    cli
.hang: hlt
    jmp .hang

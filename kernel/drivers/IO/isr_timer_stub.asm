; isr_timer_stub.asm
global isr_timer_stub
isr_timer_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_timer_handler
    call isr_timer_handler
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    leave
    iretq

; Indicate that this object file does not require an executable stack
section .note.GNU-stack noalloc nobits align=1

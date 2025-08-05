global isr_timer_stub
extern isr_timer_handler

section .text

isr_timer_stub:
    cli
    ; Save all general-purpose registers (order must match your C struct)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    ; Push interrupt vector number (for timer, usually IRQ0 = 32)
    mov rax, 32
    push rax
    ; Push dummy error code (0 for IRQs)
    xor rax, rax
    push rax
    ; Push dummy CR2 (not needed for timer, but keeps context struct uniform)
    xor rax, rax
    push rax

    ; Pass context pointer (rsp) in rdi
    mov rdi, rsp
    call isr_timer_handler

    ; Pop dummy CR2, error code, int_no
    add rsp, 24

    ; Restore registers in exact reverse order
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al

    iretq

section .note.GNU-stack noalloc nobits align=1

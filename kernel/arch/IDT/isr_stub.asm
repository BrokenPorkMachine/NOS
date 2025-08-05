global isr_timer_stub
extern isr_timer_handler

isr_timer_stub:
    cli
    ; Save general purpose registers (order must match struct)
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

    ; Push vector number (int_no)
    mov rax, 32     ; Timer IRQ vector
    push rax

    ; Push dummy error code (0 for IRQs)
    xor rax, rax
    push rax

    ; Get RIP, CS, RFLAGS, RSP, SS from stack (pushed by hardware)
    ; Optionally save cr2 (for page faults) — set to zero here
    xor rax, rax
    push rax

    ; rdi = pointer to context (rsp)
    mov rdi, rsp
    call isr_timer_handler

    ; Pop cr2
    add rsp, 8
    ; Pop error code, int_no
    add rsp, 16
    ; Pop GP registers
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

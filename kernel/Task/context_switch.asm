global context_switch
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
; rdi = pointer to save old rsp
; rsi = new rsp value

section .text
context_switch:
    ; Save current flags and disable interrupts to ensure an atomic switch
    pushfq
    cli

    ; Save callee-saved registers as per System V AMD64 ABI
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Store current stack pointer to *old_rsp
    mov [rdi], rsp

    ; Switch to the new stack
    mov rsp, rsi

    ; Restore callee-saved registers (from new context's stack)
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restore saved flags
    popfq

    ret

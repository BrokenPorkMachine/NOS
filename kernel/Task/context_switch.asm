global context_switch
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
; rdi = pointer to save old rsp
; rsi = new rsp value

section .text
context_switch:
    push rax        ; Maintain 16-byte stack alignment
    pushfq          ; Save flags
    cli             ; Disable interrupts (if kernel context switch)
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp  ; Save current stack pointer

    mov rsp, rsi    ; Switch to new stack

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    popfq           ; Restore flags (including IF)
    pop rax         ; Discard alignment placeholder

    ret

section .note.GNU-stack noalloc nobits align=1

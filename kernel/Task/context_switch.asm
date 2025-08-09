global context_switch
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
;   rdi = pointer to save old rsp (may be NULL if caller doesn't care)
;   rsi = new rsp value to load

section .text
context_switch:
    push rax        ; Maintain 16-byte alignment of the stack
    pushfq          ; Save caller's rflags and disable interrupts while switching
    cli
    ; Save callee-saved registers as per System V ABI
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Store the old stack pointer if requested
    test rdi, rdi
    jz .Lnosave
    mov [rdi], rsp
.Lnosave:

    ; Load the new stack pointer
    mov rsp, rsi

    ; Restore saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    popfq           ; Restore rflags (including IF)
    pop rax         ; Discard alignment placeholder

    ret

section .note.GNU-stack noalloc nobits align=1

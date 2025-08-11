global context_switch
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
;   rdi = pointer to save old rsp (may be NULL if caller doesn't care)
;   rsi = new rsp value to load

section .text
context_switch:
    push rax
    pushfq              ; save caller's rflags
    cli                 ; disable interrupts during the switch

    ; Save callee-saved registers (SysV)
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

    ; Restore callee-saved registers from target stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; The next slot on the target stack is its saved RFLAGS.
    ; We no longer trust flags on the stack (TF might be set), so drop it:
    add rsp, 8

    ; Set sane flags explicitly: clear TF, set IF.
    ; (Preserve other flags as much as possible.)
    pushfq
    pop rax
    and rax, ~0x100     ; ~TF
    or  rax,  0x200     ;  IF
    push rax
    popfq

    ; Continue normal restore
    pop rax
    ret

section .note.GNU-stack noalloc nobits align=1

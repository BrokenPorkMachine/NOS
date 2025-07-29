global enter_user_mode

; RDI - entry point
; RSI - user stack pointer (top)

enter_user_mode:
    mov rcx, rdi        ; entry address
    mov rax, rsi        ; user stack

    mov ax, 0x23        ; user data selector | 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push 0x23           ; SS
    push rax            ; RSP
    pushfq
    push 0x1B           ; CS (user code selector | 3)
    push rcx            ; RIP
    iretq

global enter_user_mode

; void enter_user_mode(void *entry, void *user_stack)
; Arguments:
;   rdi = entry point (RIP)
;   rsi = user stack pointer (top, for RSP)
;
; Uses GDT selectors:
;   0x23 = user data (ring 3)
;   0x1B = user code (ring 3)

section .text
enter_user_mode:
    ; Set up segment selectors for user mode
    mov  ax, 0x23         ; user data selector | 3
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Prepare iretq stack frame (SS, RSP, RFLAGS, CS, RIP)
    mov  rcx, rdi         ; entry point -> rcx
    mov  rax, rsi         ; user stack  -> rax

    push 0x23             ; SS (user data)
    push rax              ; RSP (user stack pointer)
    pushfq                ; RFLAGS (preserve IF)
    push 0x1B             ; CS (user code)
    push rcx              ; RIP (entry point)

    iretq                 ; Enter user mode!

    ; Should never return
    hlt
    jmp $


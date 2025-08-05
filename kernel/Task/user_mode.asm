%include "../arch/GDT/segments.inc"

global enter_user_mode

; void enter_user_mode(void *entry, void *user_stack)
; Arguments:
;   rdi = entry point (RIP)
;   rsi = user stack pointer (top, for RSP)
;
; Uses GDT selectors:
;   GDT_SEL_USER_DATA_R3 = user data (ring 3)
;   GDT_SEL_USER_CODE_R3 = user code (ring 3)

section .text
enter_user_mode:
    ; Set up segment selectors for user mode
    mov  ax, GDT_SEL_USER_DATA_R3
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Prepare iretq stack frame (SS, RSP, RFLAGS, CS, RIP)
    mov  rcx, rdi         ; entry point -> rcx
    mov  rax, rsi         ; user stack  -> rax

    push GDT_SEL_USER_DATA_R3
    push rax              ; RSP (user stack pointer)
    pushfq                ; RFLAGS (preserve IF)
    push GDT_SEL_USER_CODE_R3
    push rcx              ; RIP (entry point)

    iretq                 ; Enter user mode!

    ; Should never return
    hlt
    jmp $

; Indicate that this object file does not require an executable stack
section .note.GNU-stack noalloc nobits align=1


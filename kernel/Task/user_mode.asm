%include "../arch/GDT/segments.inc"

global enter_user_mode

; void enter_user_mode(void *entry, void *user_stack)
; Arguments:
;   rdi = entry point (user RIP)
;   rsi = user stack pointer (user RSP)
;
; GDT Selectors (should be 0x23 for data, 0x1B for code with RPL=3)
;   GDT_SEL_USER_DATA_R3  equ 0x23
;   GDT_SEL_USER_CODE_R3  equ 0x1B

section .text
enter_user_mode:
    ; Set user-mode segment registers
    mov  ax, GDT_SEL_USER_DATA_R3
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Load arguments
    mov  rcx, rdi         ; rcx = user entry point (RIP)
    mov  rax, rsi         ; rax = user stack pointer (RSP)

    ; Prepare iretq frame: SS, RSP, RFLAGS, CS, RIP (in that order, bottom to top)
    push GDT_SEL_USER_DATA_R3  ; User SS
    push rax                   ; User RSP
    pushfq                     ; RFLAGS (current flags, but be sure IF=1)
    push GDT_SEL_USER_CODE_R3  ; User CS
    push rcx                   ; User RIP

    ; Double-check: ensure IF=1 (interrupts enabled) in RFLAGS
    ; This is optional, but safest if you want user code to see interrupts
    ; (Uncomment if needed:)
    ; pop rdx
    ; or  rdx, 0x200      ; Set IF
    ; push rdx

    ; Far return to user mode, privilege transition!
    iretq

    ; Should never return; just in case, halt forever
.dead:
    hlt
    jmp .dead

section .note.GNU-stack noalloc nobits align=1

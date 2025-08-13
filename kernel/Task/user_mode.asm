%include "../arch/GDT/segments.inc"

global enter_user_mode

; void enter_user_mode(void *entry, void *user_stack)
;   rdi = user RIP
;   rsi = user RSP (ideally 16-byte aligned)
; GDT_SEL_USER_DATA_R3 = 0x1B, GDT_SEL_USER_CODE_R3 = 0x23

section .text
enter_user_mode:
    ; Stash args
    mov  rcx, rdi     ; rcx = user RIP
    mov  rax, rsi     ; rax = user RSP (should be canonical & 16B aligned)

    ; Build iretq frame: SS,RSP,RFLAGS,CS,RIP
    push GDT_SEL_USER_DATA_R3   ; SS (RPL=3)
    push rax                    ; RSP
    mov  rdx, 0x202             ; RFLAGS with IF set
    push rdx                    ; RFLAGS
    push GDT_SEL_USER_CODE_R3   ; CS (RPL=3, 64-bit)
    push rcx                    ; RIP

    iretq                       ; drop to user mode, never returns

.dead:
    hlt
    jmp .dead

section .note.GNU-stack noalloc nobits align=1

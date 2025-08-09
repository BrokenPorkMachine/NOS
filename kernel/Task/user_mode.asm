%include "../arch/GDT/segments.inc"

global enter_user_mode

; void enter_user_mode(void *entry, void *user_stack)
;   rdi = user RIP
;   rsi = user RSP (ideally 16-byte aligned)
; GDT_SEL_USER_DATA_R3 = 0x23, GDT_SEL_USER_CODE_R3 = 0x1B

section .text
enter_user_mode:
    ; Load RPL=3 segments. In long mode DS/ES are ignored; SS DPL matters.
    mov  ax, GDT_SEL_USER_DATA_R3
    mov  ds, ax
    mov  es, ax
    mov  fs, ax       ; selector only; FS base is via MSR_FS_BASE if using TLS
    mov  gs, ax       ; selector only; GS base via MSR_GS_BASE/KERNEL_GS_BASE
    mov  ss, ax

    ; Stash args
    mov  rcx, rdi     ; rcx = user RIP
    mov  rax, rsi     ; rax = user RSP (should be canonical & 16B aligned)

    ; iretq frame (SS,RSP,RFLAGS,CS,RIP). iretq pops in reverse into user mode.
    push GDT_SEL_USER_DATA_R3   ; SS (RPL=3)
    push rax                    ; RSP
    mov  rdx, 0x202             ; RFLAGS: IF=1, others cleared
    push rdx                    ; RFLAGS
    push GDT_SEL_USER_CODE_R3   ; CS (RPL=3, 64-bit)
    push rcx                    ; RIP

    iretq                       ; drop to user mode, never returns

.dead:
    hlt
    jmp .dead

section .note.GNU-stack noalloc nobits align=1

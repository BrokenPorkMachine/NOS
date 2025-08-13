; NASM uses the current working directory (the repository root during the
; build) as the base for relative includes.  The previous path assumed the
; assembler executed from within `kernel/Task`, causing the include to fail
; when run from the root.  Use an explicit path from the repository root so
; the file is found regardless of invocation location.
%include "kernel/arch/GDT/segments.inc"

global enter_user_mode
extern assert_gdt_selector

; void enter_user_mode(void *entry, void *user_stack)
;   rdi = user RIP
;   rsi = user RSP (ideally 16-byte aligned)
; GDT_SEL_USER_CODE_R3 = 0x1B, GDT_SEL_USER_DATA_R3 = 0x23

section .text
enter_user_mode:
    ; Scrub segment registers to known-good kernel data selector
    mov  ax, GDT_SEL_KERNEL_DATA
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Stash args in callee-saved regs
    mov  rbx, rdi     ; rbx = user RIP
    mov  r12, rsi     ; r12 = user RSP (should be canonical & 16B aligned)

    ; Assert selectors use GDT (TI=0)
    mov  di, GDT_SEL_USER_DATA_R3
    lea  rsi, [rel .str_ss]
    call assert_gdt_selector
    mov  di, GDT_SEL_USER_CODE_R3
    lea  rsi, [rel .str_cs]
    call assert_gdt_selector

    ; Build iretq frame: SS,RSP,RFLAGS,CS,RIP
    push GDT_SEL_USER_DATA_R3   ; SS (RPL=3)
    push r12                    ; RSP
    pushfq                      ; RFLAGS
    or   qword [rsp], (1<<9)    ; ensure IF=1
    push GDT_SEL_USER_CODE_R3   ; CS (RPL=3, 64-bit)
    push rbx                    ; RIP

    iretq                       ; drop to user mode, never returns

.dead:
    hlt
    jmp .dead

section .rodata
.str_ss: db "enter_user_mode SS",0
.str_cs: db "enter_user_mode CS",0

section .note.GNU-stack noalloc nobits align=1

%include "kernel/arch/GDT/segments.inc"

extern assert_selector_gdt

global switch_to_user

section .text
switch_to_user:
    ; Save user RIP/RSP across calls
    mov rbx, rdi        ; user RIP
    mov r12, rsi        ; user RSP

    ; Load user data selector into data segments
    mov ax, GDT_SEL_USER_DATA_R3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Verify selectors use GDT
    mov di, GDT_SEL_USER_DATA_R3
    lea rsi, [rel .str_ss]
    call assert_selector_gdt
    mov di, GDT_SEL_USER_CODE_R3
    lea rsi, [rel .str_cs]
    call assert_selector_gdt

    ; Build iretq frame and enter user mode
    push GDT_SEL_USER_DATA_R3   ; SS
    push r12                    ; RSP
    pushfq                      ; RFLAGS
    or   qword [rsp], (1<<9)    ; ensure IF=1
    push GDT_SEL_USER_CODE_R3   ; CS
    push rbx                    ; RIP
    iretq                       ; never returns

.hang:
    hlt
    jmp .hang

section .rodata
.str_ss: db "switch_to_user SS",0
.str_cs: db "switch_to_user CS",0

section .note.GNU-stack noalloc nobits align=1

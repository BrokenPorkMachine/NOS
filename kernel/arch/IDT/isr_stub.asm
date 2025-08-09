; ISR stubs for all vectors. Each stub saves the register state in the order
; expected by struct isr_context and then invokes the appropriate C handler.

global isr_timer_stub
global isr_keyboard_stub
global isr_mouse_stub
global isr_page_fault_stub
global isr_gpf_stub
global isr_syscall_stub
global isr_ipi_stub
global isr_stub_table

extern isr_default_handler
extern isr_timer_handler
extern isr_keyboard_handler
extern isr_mouse_handler
extern isr_page_fault_handler
extern isr_gpf_handler
extern isr_syscall_handler
extern isr_ipi_handler

%macro PUSH_REGS 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax
%endmacro

%macro POP_REGS 0
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro

%macro ISR_DEFAULT 1
global isr_stub_%1
isr_stub_%1:
    cli
    PUSH_REGS
    mov rax, %1
    push rax           ; int_no
    xor rax, rax
    push rax           ; error code
    push rax           ; cr2 (unused)
    mov al, 0x20
    out 0x20, al
    mov rdi, rsp
    call isr_default_handler
.hang%1:
    hlt
    jmp .hang%1
%endmacro

%macro ISR_ERROR 1
global isr_stub_%1
isr_stub_%1:
    cli
    PUSH_REGS
    mov rax, [rsp + 120]
    mov rbx, [rsp + 128]
    mov [rsp + 120], rbx
    mov rbx, [rsp + 136]
    mov [rsp + 128], rbx
    mov rbx, [rsp + 144]
    mov [rsp + 136], rbx
    mov rbx, [rsp + 152]
    mov [rsp + 144], rbx
    mov rbx, [rsp + 160]
    mov [rsp + 152], rbx
    mov rcx, %1
    push rcx               ; int_no
    push rax               ; error code
    xor rax, rax
    push rax               ; cr2 (unused)
    mov al, 0x20
    out 0x20, al
    mov rdi, rsp
    call isr_default_handler
.hang_err%1:
    hlt
    jmp .hang_err%1
%endmacro

%assign i 0
%rep 256
%if i = 8 || i = 10 || i = 11 || i = 12 || i = 17
    ISR_ERROR i
%elif i != 32 && i != 33 && i != 44 && i != 13 && i != 14 && i != 0x80 && i != 0xF0
    ISR_DEFAULT i
%endif
%assign i i+1
%endrep

isr_gpf_stub:
    cli
    PUSH_REGS
    mov rax, [rsp + 120]
    mov rbx, [rsp + 128]
    mov [rsp + 120], rbx
    mov rbx, [rsp + 136]
    mov [rsp + 128], rbx
    mov rbx, [rsp + 144]
    mov [rsp + 136], rbx
    mov rbx, [rsp + 152]
    mov [rsp + 144], rbx
    mov rbx, [rsp + 160]
    mov [rsp + 152], rbx
    mov rcx, 13
    push rcx
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_gpf_handler
    add rsp, 24
    POP_REGS
    iretq

isr_timer_stub:
    cli
    PUSH_REGS
    mov rax, 32
    push rax
    xor rax, rax
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_timer_handler
    add rsp, 24
    POP_REGS
    mov al, 0x20
    out 0x20, al
    iretq

isr_keyboard_stub:
    cli
    PUSH_REGS
    mov rax, 33
    push rax
    xor rax, rax
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_keyboard_handler
    add rsp, 24
    POP_REGS
    mov al, 0x20
    out 0x20, al
    iretq

isr_mouse_stub:
    cli
    PUSH_REGS
    mov rax, 44
    push rax
    xor rax, rax
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_mouse_handler
    add rsp, 24
    POP_REGS
    mov al, 0x20
    out 0x20, al
    iretq

isr_page_fault_stub:
    cli
    PUSH_REGS
    mov rax, [rsp + 120]
    mov rdx, cr2
    mov rcx, 14
    push rcx
    push rax
    push rdx
    mov rdi, rsp
    call isr_page_fault_handler
    add rsp, 24
    POP_REGS
    iretq

isr_syscall_stub:
    cli
    PUSH_REGS
    mov rax, 0x80
    push rax
    xor rax, rax
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_syscall_handler
    add rsp, 24
    POP_REGS
    iretq

isr_ipi_stub:
    cli
    PUSH_REGS
    mov rax, 0xF0
    push rax
    xor rax, rax
    push rax
    xor rax, rax
    push rax
    mov rdi, rsp
    call isr_ipi_handler
    add rsp, 24
    POP_REGS
    iretq

section .note.GNU-stack noalloc nobits align=1

section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
%if i = 32
    dq isr_timer_stub
%elif i = 33
    dq isr_keyboard_stub
%elif i = 44
    dq isr_mouse_stub
%elif i = 13
    dq isr_gpf_stub
%elif i = 14
    dq isr_page_fault_stub
%elif i = 0x80
    dq isr_syscall_stub
%elif i = 0xF0
    dq isr_ipi_stub
%else
    dq isr_stub_%+i
%endif
%assign i i+1
%endrep

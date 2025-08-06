global isr_timer_stub
global isr_keyboard_stub
global isr_mouse_stub
global isr_page_fault_stub
global isr_syscall_stub
global isr_ipi_stub
global isr_stub_table

extern isr_default_handler
extern isr_timer_handler
extern isr_keyboard_handler
extern isr_mouse_handler
extern isr_page_fault_handler
extern isr_syscall_handler
extern isr_ipi_handler

%macro ISR_DEFAULT 1
global isr_stub_%1
isr_stub_%1:
    cli
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

    mov rax, %1
    push rax          ; int_no
    xor rax, rax
    push rax          ; error code
    push rax          ; cr2 (unused)

    mov al, 0x20
    out 0x20, al

    mov rdi, rsp
    call isr_default_handler

.hang%1:
    hlt
    jmp .hang%1
%endmacro

%assign i 0
%rep 256
%if i != 32 && i != 33 && i != 44 && i != 14 && i != 0x80 && i != 0xF0
    ISR_DEFAULT i
%endif
%assign i i+1
%endrep

isr_timer_stub:
    cli
    ; Save general purpose registers (order must match struct)
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

    ; Push vector number (int_no)
    mov rax, 32     ; Timer IRQ vector
    push rax

    ; Push dummy error code (0 for IRQs)
    xor rax, rax
    push rax

    ; Get RIP, CS, RFLAGS, RSP, SS from stack (pushed by hardware)
    ; Optionally save cr2 (for page faults) — set to zero here
    xor rax, rax
    push rax

    ; rdi = pointer to context (rsp)
    mov rdi, rsp
    call isr_timer_handler

    ; Pop cr2
    add rsp, 8
    ; Pop error code, int_no
    add rsp, 16
    ; Pop GP registers
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

    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al

    iretq

isr_keyboard_stub:
    cli
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

    mov rax, 33
    push rax
    xor rax, rax
    push rax
    push rax

    mov rdi, rsp
    call isr_keyboard_handler

    add rsp, 8
    add rsp, 16

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

    mov al, 0x20
    out 0x20, al

    iretq

isr_mouse_stub:
    cli
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

    mov rax, 44
    push rax
    xor rax, rax
    push rax
    push rax

    mov rdi, rsp
    call isr_mouse_handler

    add rsp, 8
    add rsp, 16

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

    mov al, 0x20
    out 0x20, al

    iretq

isr_page_fault_stub:
    cli
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

    ; Load error code and cr2
    mov rax, [rsp + 120]
    mov rdx, cr2

    mov rcx, 14
    push rcx
    push rax
    push rdx

    mov rdi, rsp
    call isr_page_fault_handler

    add rsp, 24

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

    iretq

isr_syscall_stub:
    cli
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

    mov rax, 0x80
    push rax
    xor rax, rax
    push rax
    push rax

    mov rdi, rsp
    call isr_syscall_handler

    add rsp, 8
    add rsp, 16

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

    iretq

isr_ipi_stub:
    cli
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

    mov rax, 0xF0
    push rax
    xor rax, rax
    push rax
    push rax

    mov rdi, rsp
    call isr_ipi_handler

    add rsp, 8
    add rsp, 16

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

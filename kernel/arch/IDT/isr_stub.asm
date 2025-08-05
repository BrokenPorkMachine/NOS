; Mark all external handlers
extern isr_timer_handler
extern schedule_from_isr
extern isr_syscall_handler
extern isr_default_handler
extern isr_keyboard_handler
extern isr_mouse_handler
extern isr_page_fault_handler
extern isr_ipi_handler

section .text

; -------------------------------
global isr_timer_stub
isr_timer_stub:
    cli
    ; --- Save all GP registers
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; --- Maintain stack alignment: 15 pushes + iret frame (pushed by CPU) = 16
    ; --- Call handler (handler must preserve registers if it uses them)
    call isr_timer_handler

    mov rdi, rsp             ; Pass pointer to saved context
    call schedule_from_isr
    mov rsp, rax             ; Switch to new thread stack

    ; --- EOI to master PIC
    mov al, 0x20
    out 0x20, al

    ; --- Restore all GP registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    iretq

; -------------------------------
global isr_syscall_stub
isr_syscall_stub:
    ; Note: you usually DO NOT cli in syscall handlers (leave interrupts enabled!)
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rbp, rsp   ; Save stack pointer for possible switching/context

    call isr_syscall_handler

    mov rsp, rbp   ; Restore original stack pointer

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    iretq

; -------------------------------
global isr_default_stub
isr_default_stub:
    push rbp
    mov rbp, rsp
    cli
    mov rdi, rsp
    call isr_default_handler
    leave
    iretq

; -------------------------------
global isr_keyboard_stub
isr_keyboard_stub:
    cli
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call isr_keyboard_handler

    mov al, 0x20
    out 0x20, al

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    iretq

; -------------------------------
global isr_mouse_stub
isr_mouse_stub:
    cli
    push rbp
    mov rbp, rsp

    call isr_mouse_handler

    mov al, 0x20
    out 0xA0, al   ; EOI to slave
    out 0x20, al   ; EOI to master

    leave
    iretq

; -------------------------------
global isr_page_fault_stub
isr_page_fault_stub:
    cli
    push rax
    mov rax, cr2
    push rax                 ; Save fault address
    mov rdi, [rsp+16]        ; Error code (CPU pushes error code before RIP)
    mov rsi, [rsp]           ; Fault address
    call isr_page_fault_handler
    add rsp, 8               ; Remove fault address
    pop rax
    iretq

; -------------------------------
global isr_ipi_stub
isr_ipi_stub:
    cli
    push rbp
    mov rbp, rsp

    call isr_ipi_handler

    leave
    iretq

; -------------------------------
section .note.GNU-stack noalloc nobits align=1

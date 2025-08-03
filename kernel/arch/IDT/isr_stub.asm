global isr_timer_stub
isr_timer_stub:
    push rbp
    mov rbp, rsp
    extern isr_timer_handler
    call isr_timer_handler
    ; Acknowledge the PIC before switching threads
    mov al, 0x20
    out 0x20, al
    ; Yield to the scheduler to pick the next thread
    extern thread_yield
    call thread_yield
    ; Execution resumes here when the preempted thread runs again
    leave
    iretq


global isr_syscall_stub
isr_syscall_stub:
    cli
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbp
    mov rbp, rsp
    extern isr_syscall_handler
    call isr_syscall_handler
    mov rsp, rbp
    pop rbp
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 8 ; discard saved rax
    sti
    iretq

global isr_default_stub
isr_default_stub:
    push rbp
    mov rbp, rsp
    cli
    mov rdi, rsp
    extern isr_default_handler
    call isr_default_handler
    leave
    iretq

global isr_keyboard_stub
isr_keyboard_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_keyboard_handler
    call isr_keyboard_handler
    mov al, 0x20
    out 0x20, al
    leave
    iretq

global isr_mouse_stub
isr_mouse_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_mouse_handler
    call isr_mouse_handler
    mov al, 0x20
    out 0x20, al
    leave
    iretq

global isr_page_fault_stub
isr_page_fault_stub:
    cli
    push rax
    mov rax, cr2
    push rax            ; fault address
    mov rdi, [rsp+16]   ; error code
    mov rsi, [rsp]      ; address
    extern isr_page_fault_handler
    call isr_page_fault_handler
    add rsp, 8          ; pop address
    pop rax
    iretq

global isr_ipi_stub
isr_ipi_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_ipi_handler
    call isr_ipi_handler
    leave
    iretq

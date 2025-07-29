global isr_timer_stub
isr_timer_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_timer_handler
    call isr_timer_handler
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    leave
    iretq


global isr_syscall_stub
isr_syscall_stub:
    push rbp
    mov rbp, rsp
    cli
    extern isr_syscall_handler
    call isr_syscall_handler
    leave
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

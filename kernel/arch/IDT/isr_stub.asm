BITS 64
default rel

global isr_stub_table
global isr_ud_stub
global isr_timer_stub

extern lapic_eoi
extern isr_timer_handler   ; void isr_timer_handler(const void *hw_frame)

section .text

isr_ud_stub:
    cli
.ud_hang:
    hlt
    jmp .ud_hang

; HW pushes {RIP, CS, RFLAGS} for kernel->kernel interrupts.
; We pass a pointer to that frame (opaque) into the C handler.
isr_timer_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    lea  rdi, [rsp + 9*8]      ; rdi = &HW frame
    call isr_timer_handler

    call lapic_eoi

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

section .rodata
align 8
isr_stub_table:
%assign i 0
%rep 256
    dq isr_ud_stub
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits

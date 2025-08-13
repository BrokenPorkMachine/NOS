; kernel/arch/IDT/isr_stub.asm
; Build with: nasm -f elf64 kernel/arch/IDT/isr_stub.asm -o kernel/arch/IDT/isr_stub.o
BITS 64
default rel

global isr_stub_table
global isr_ud_stub
global isr_timer_stub

extern lapic_eoi

section .text

; #UD (vector 6) — no error code
isr_ud_stub:
    cli
.ud_hang:
    hlt
    jmp .ud_hang

; APIC Timer (vector 32) — no error code
isr_timer_stub:
    push rax
    push rcx
    push rdx
    call lapic_eoi
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

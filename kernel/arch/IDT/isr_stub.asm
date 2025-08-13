; kernel/arch/IDT/isr_stub.asm
; Minimal 64-bit ISR stubs for NASM

BITS 64
default rel

global isr_stub_table
global isr_ud_stub
global isr_timer_stub

extern lapic_eoi    ; provided by your LAPIC driver

section .text

; ---------------------------
; #UD (vector 6) — no errcode
; ---------------------------
isr_ud_stub:
    cli
.ud_hang:
    hlt
    jmp .ud_hang

; ---------------------------------
; APIC Timer (assume vector 32/0x20)
; No errcode
; ---------------------------------
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

; ---------------------------------
; Stub table: 256 entries
; default all to isr_ud_stub; C overrides the ones you use
; ---------------------------------
isr_stub_table:
%assign i 0
%rep 256
    dq isr_ud_stub
%assign i i+1
%endrep

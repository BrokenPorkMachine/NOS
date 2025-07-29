; GDT assembly routines

; Flushes new GDT and reloads segment registers
; rdi - pointer to gdt_ptr structure

global gdt_flush

section .text

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10       ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; far jump to reload CS
    push 0x08
    lea rax, [rel .flush]
    push rax
    retfq
.flush:
    ret

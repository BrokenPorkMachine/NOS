; GDT assembly routines
; Flushes new GDT and reloads segment registers
; rdi = pointer to gdt_ptr structure

%include "segments.inc"            ; :contentReference[oaicite:14]{index=14}

global gdt_flush
section .text

gdt_flush:
    lgdt [rdi]
    mov ax, GDT_SEL_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; Far jump to reload CS
    push GDT_SEL_KERNEL_CODE
    lea rax, [rel .flush]
    push rax
    retfq
.flush:
    ret

section .note.GNU-stack noalloc nobits align=1

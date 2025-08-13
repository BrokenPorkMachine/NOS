; GDT assembly routines (x86_64, NASM/YASM)
; rdi = pointer to gdt_ptr { uint16_t limit; uint64_t base; }
; Optional: gdt_flush_with_tr also loads TR from 'si' (16-bit selector)

%include "segments.inc"

global gdt_flush
global gdt_flush_with_tr

section .text

; ----------------------------------------------------------------------
; void gdt_flush(const struct gdtr *p);
; Loads a new GDT, reloads data segments and CS via far return.
; Clobbers: rax (preserved on exit)
; ----------------------------------------------------------------------
gdt_flush:
    push rax
    lgdt [rdi]

    ; Ensure LDT is cleared and prove it's unused
    xor eax, eax
    sldt ax            ; read current LDTR
    test ax, ax
    jz .no_ldt_gdt
    xor eax, eax
    lldt ax            ; force LDTR = 0
.no_ldt_gdt:

    ; Reload data segments (long mode: DS/ES ignored for addressing but keep sane)
    mov ax, 0x10              ; GDT_SEL_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far return to reload CS with kernel code selector (0x08)
    lea rax, [rel .after]
    push qword 0x08           ; GDT_SEL_KERNEL_CODE
    push rax
    retfq
.after:
    pop rax
    ret

; ----------------------------------------------------------------------
; void gdt_flush_with_tr(const struct gdtr *p, uint16_t tss_sel);
; Loads GDT, reloads segments, loads TR with the provided TSS selector,
; then reloads CS via far return.
; Clobbers: rax, rdx, rcx  (preserved on exit)
; ----------------------------------------------------------------------
gdt_flush_with_tr:
    push rax
    push rdx
    push rcx

    lgdt [rdi]

    ; Ensure LDT is cleared and prove it's unused
    xor eax, eax
    sldt ax            ; read current LDTR
    test ax, ax
    jz .no_ldt_tr
    xor eax, eax
    lldt ax            ; force LDTR = 0
.no_ldt_tr:

    mov ax, GDT_SEL_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load task register (TSS selector in SI)
    mov ax, si
    ltr ax

    ; Far return to reload CS
    lea rax, [rel .after_flush_cs_tr]
    push qword GDT_SEL_KERNEL_CODE
    push rax
    retfq

.after_flush_cs_tr:
    pop rcx
    pop rdx
    pop rax
    ret

section .note.GNU-stack noalloc nobits align=1

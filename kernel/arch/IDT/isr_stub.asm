/* isr_stubs.S */
.intel_syntax noprefix
.global isr_stub_table
.global isr_ud_stub
.global isr_timer_stub

/* You should provide these from your APIC/PIC driver if using the EOI path */
.extern lapic_eoi

/* For simplicity, we build two hand-written stubs and a table pointing to them.
   In a full kernel, you'd macro-generate one per vector. */

.section .text

/* #UD (vector 6) — no error code */
isr_ud_stub:
    /* TODO: log registers or call into your #UD handler if you have one.
       For now, just hang to make it obvious. */
.ud_hang:
    cli
    hlt
    jmp .ud_hang

/* APIC Timer (let’s assume vector 32) — no error code */
isr_timer_stub:
    /* Acknowledge LAPIC EOI (MMIO write), if you have a function; otherwise inline */
    push rax
    push rcx
    push rdx
    /* lapic_eoi(); */
    call lapic_eoi
    pop rdx
    pop rcx
    pop rax
    iretq

/* Stub table: point all entries to a safe default; override key ones in C if desired */
.section .rodata
.align 8
isr_stub_table:
    /* 0..255 */
    .rept 256
        .quad isr_ud_stub
    .endr

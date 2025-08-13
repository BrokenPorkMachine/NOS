; ISR stubs for all vectors (x86_64)
; Contract:
;   - Save GPRs in PUSH_REGS order
;   - Push synthetic: [int_no][error_code][cr2_or_0]
;   - Pass RDI = RSP (points to isr_context on stack)
;   - Call C handler
;   - Pop synthetic 3 * 8
;   - Pop GPRs
;   - If CPU pushed an error code for this vector, add RSP, 8 before IRETQ
;   - EOI is done in C via LAPIC (no PIC EOI by default)

; Uncomment if you really run legacy PIC and want EOIs from the stub:
;%define USE_PIC_EOI 0

global isr_timer_stub
global isr_keyboard_stub
global isr_mouse_stub
global isr_page_fault_stub
global isr_gpf_stub
global isr_syscall_stub
global isr_ipi_stub
global isr_ud_stub
global isr_stub_table

extern isr_default_handler
extern isr_timer_handler
extern isr_keyboard_handler
extern isr_mouse_handler
extern isr_page_fault_handler
extern isr_gpf_handler
extern isr_syscall_handler
extern isr_ipi_handler
extern isr_ud_handler

%macro PUSH_REGS 0
    ; Preserve GPRs (matches your struct isr_context order)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax
%endmacro

%macro POP_REGS 0
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro

%macro PUSH_SYNTH 3
    ; Push our synthetic (int_no, error_code, cr2)
    ; %1 = int_no, %2 = error_code (register/imm), %3 = cr2_val (register/imm)
    mov rax, %1
    push rax              ; int_no
    mov rax, %2
    push rax              ; error_code
    mov rax, %3
    push rax              ; cr2 (or 0)
%endmacro

%macro POP_SYNTH 0
    add rsp, 24           ; drop int_no/error/cr2
%endmacro

; Determine if vector pushes an error code (exceptions: 8,10,11,12,13,14,17)
%macro VEC_HAS_ERR 2
    ; sets %2 (a label suffix) path
%endmacro

; Compute the saved error code in CPU frame:
; After PUSH_REGS (15*8 bytes), hardware frame starts at [rsp + 120].
; Layout (no CPL change): [RIP][CS][RFLAGS][ERROR?]
; With CPL change:        [RIP][CS][RFLAGS][SS][RSP][ERROR?]
; We detect CPL change by checking saved CS.RPL.
%macro LOAD_HW_ERR 1
    ; out: %1 gets error code (R64)
    ; tmp: rdx, rbx clobbered
    lea rdx, [rsp + 120]           ; RDX -> saved RIP
    mov rbx, [rdx + 8]             ; saved CS
    mov %1, [rdx + 24]             ; provisional: this is RFLAGS, but we only need CS RPL
    xor %1, %1                     ; clear output
    test bl, 3
    jz %%no_cpl_change
    ; CPL change -> SS,RSP are present => ERROR is 24 + 16 = 40 bytes from RIP
    mov %1, [rdx + 40]
    jmp %%got_err
%%no_cpl_change:
    ; No CPL change -> ERROR is 24 bytes from RIP
    mov %1, [rdx + 24]
%%got_err:
%endmacro

; --- Default handler generator for vectors w/o CPU error code ---
%macro ISR_NOERR 2
; %1 = vec number
; %2 = C handler symbol
global isr_stub_%1
isr_stub_%1:
    PUSH_REGS
    ; cr2 not meaningful here; push 0
    PUSH_SYNTH %1, 0, 0
    mov rdi, rsp
    call %2
    POP_SYNTH
    POP_REGS
%ifdef USE_PIC_EOI
    ; legacy PIC EOI for IRQ lines only (typical 32-47); harmless for others
    cmp byte [rel .is_irq_%1], 0
    je .skip_pic_eoi_%1
    mov al, 0x20
    out 0x20, al
.skip_pic_eoi_%1:
%endif
    iretq
%ifdef USE_PIC_EOI
.is_irq_%1: db (%1 >= 32) & (%1 <= 47)
%endif
%endmacro

; --- Handler generator for vectors with CPU-pushed error code ---
%macro ISR_HASERR 2
; %1 = vec number
; %2 = C handler symbol
global isr_stub_%1
isr_stub_%1:
    PUSH_REGS
    ; Load hardware-pushed error code from CPU frame
    LOAD_HW_ERR rax
    ; cr2 meaningful for #PF only (vec 14); use CR2 else 0
%if %1 = 14
    mov rdx, cr2
%else
    xor rdx, rdx
%endif
    PUSH_SYNTH %1, rax, rdx
    mov rdi, rsp
    call %2
    POP_SYNTH
    POP_REGS
    ; Discard hardware-pushed error code before returning
    add rsp, 8
    iretq
%endmacro

isr_ud_stub:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    mov rax, 6
    push rax
    xor  rax, rax
    push rax
    push rax

    mov rdi, rsp
    call isr_ud_handler

    add rsp, 24
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq

; --- Specific leaf stubs ------------------------------------------------------

; Error-code exceptions
ISR_HASERR 8,  isr_default_handler       ; #DF -> usually special IST; C may panic
ISR_HASERR 10, isr_default_handler       ; #TS
ISR_HASERR 11, isr_default_handler       ; #NP
ISR_HASERR 12, isr_default_handler       ; #SS
ISR_HASERR 13, isr_gpf_handler           ; #GP
ISR_HASERR 14, isr_page_fault_handler    ; #PF (reads CR2)
ISR_HASERR 17, isr_default_handler       ; #AC

; Common IRQs and no-error exceptions
ISR_NOERR  32, isr_timer_handler
ISR_NOERR  33, isr_keyboard_handler
ISR_NOERR  44, isr_mouse_handler
ISR_NOERR  128, isr_default_handler     ; int 0x80 compat
ISR_NOERR  240, isr_ipi_handler         ; IPI

; Generate defaults for the rest
%assign i 0
%rep 256
%if i != 6 && i != 8 && i != 10 && i != 11 && i != 12 && i != 13 && i != 14 && i != 17 && i != 32 && i != 33 && i != 44 && i != 0x80 && i != 0xF0
    ISR_NOERR i, isr_default_handler
%endif
%assign i i+1
%endrep

section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
%if i = 32
    dq isr_stub_32
%elif i = 33
    dq isr_stub_33
%elif i = 6
    dq isr_ud_stub
%elif i = 44
    dq isr_stub_44
%elif i = 8
    dq isr_stub_8
%elif i = 10
    dq isr_stub_10
%elif i = 11
    dq isr_stub_11
%elif i = 12
    dq isr_stub_12
%elif i = 13
    dq isr_stub_13
%elif i = 14
    dq isr_stub_14
%elif i = 17
    dq isr_stub_17
%elif i = 0x80
    dq isr_stub_128
%elif i = 0xF0
    dq isr_stub_240
%else
    dq isr_stub_%+i
%endif
%assign i i+1
%endrep

section .note.GNU-stack noalloc nobits align=1

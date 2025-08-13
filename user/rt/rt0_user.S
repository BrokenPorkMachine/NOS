; ============================================================================
; user/rt/rt0_user.asm  -- Universal CRT0 for NOS
; Supports both:
;   - Agents: init_main(const AgentAPI*, uint32_t)
;   - Apps:   main(void)
; Loader passes:
;   RDI = api, RSI = tid    (register ABI)  OR
;   [ret][api][tid]         (stack ABI)
; ============================================================================
BITS 64
default rel

extern __init_array_start
extern __init_array_end
extern __fini_array_start
extern __fini_array_end

; We declare both as optional (weak) — linker will pick what's available
extern init_main
extern main

section .data align=8
global NOS
NOS:        dq 0

global NOS_TID
NOS_TID:    dd 0
            dd 0                ; padding to keep alignment

section .text align=16

; ----------------------------------------------------------------------------
; run_init_array
; ----------------------------------------------------------------------------
run_init_array:
    lea     rdi, [__init_array_start]
    lea     rsi, [__init_array_end]
    cmp     rdi, rsi
    jae     .done
.loop:
    cmp     rdi, rsi
    jae     .done
    mov     rax, [rdi]
    add     rdi, 8
    test    rax, rax
    jz      .loop
    call    rax
    jmp     .loop
.done:
    ret

; ----------------------------------------------------------------------------
; run_fini_array
; ----------------------------------------------------------------------------
run_fini_array:
    lea     rdi, [__fini_array_start]
    lea     rsi, [__fini_array_end]
    cmp     rdi, rsi
    jae     .done
.loop:
    cmp     rdi, rsi
    jae     .done
    sub     rsi, 8
    mov     rax, [rsi]
    test    rax, rax
    jz      .loop
    call    rax
    jmp     .loop
.done:
    ret

; ----------------------------------------------------------------------------
; universal entry helper
; ----------------------------------------------------------------------------
call_user_entry:
    ; Try init_main(api, tid) if defined, else main()
    mov     rax, init_main
    test    rax, rax
    jnz     .call_init
    mov     rax, main
    test    rax, rax
    jz      .no_entry         ; nothing defined
    call    rax
    ret
.call_init:
    mov     rdi, [rel NOS]
    mov     esi, dword [rel NOS_TID]
    call    rax
    ret
.no_entry:
    ret

; ----------------------------------------------------------------------------
; _start: register ABI (RDI = api, RSI = tid)
; ----------------------------------------------------------------------------
global _start
_start:
    and     rsp, -16
    mov     [rel NOS], rdi
    mov     [rel NOS_TID], esi
    call    run_init_array
    call    call_user_entry
    call    run_fini_array
.halt:
    hlt
    jmp     .halt

; ----------------------------------------------------------------------------
; _start_stack: stack ABI ([ret][api][tid])
; ----------------------------------------------------------------------------
global _start_stack
_start_stack:
    mov     rdi, [rsp + 8]    ; api
    mov     rsi, [rsp + 16]   ; tid
    and     rsp, -16
    mov     [rel NOS], rdi
    mov     [rel NOS_TID], esi
    call    run_init_array
    call    call_user_entry
    call    run_fini_array
.halt2:
    hlt
    jmp     .halt2

section .note.GNU-stack noalloc noexec nowrite progbits

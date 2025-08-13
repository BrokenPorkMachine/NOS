; ============================================================================
; user/rt/rt0_user.asm  -- NASM x86-64 CRT0 for NOS agents
; Exposes:
;   _start        ; loader passes RDI=api, RSI=tid  (SysV registers)
;   _start_stack  ; loader pushed [ret][api][tid]   (stack ABI)
; Defines globals (match agent_abi.h):
;   global NOS:    dq 0
;   global NOS_TID:dd 0 (plus padding)
; Calls:
;   extern init_main   ; void init_main(const AgentAPI*, uint32_t)
; Also runs .init_array/.fini_array if present.
; ============================================================================

BITS 64
default rel

extern init_main
; These usually come from the linker script, even if empty.
extern __init_array_start
extern __init_array_end
extern __fini_array_start
extern __fini_array_end

section .data align=8
global NOS
NOS:        dq 0

global NOS_TID
NOS_TID:    dd 0
            dd 0                ; pad to 8 bytes (keeps alignment nice)

section .text align=16

; ----------------------------------------------------------------------------
; Helpers: run .init_array
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

; Helpers: run .fini_array in reverse
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
; _start: register ABI (RDI = api, RSI = tid)
; ----------------------------------------------------------------------------
global _start
_start:
    ; Keep SysV 16-byte alignment before any call
    and     rsp, -16

    ; Save API/TID into globals
    mov     [rel NOS], rdi          ; NOS = api
    mov     [rel NOS_TID], esi      ; NOS_TID = (uint32)tid

    ; Run constructors (if any)
    call    run_init_array

    ; Call user's C entry: init_main(api, tid)
    mov     rdi, [rel NOS]
    mov     esi, dword [rel NOS_TID]
    call    init_main

    ; If it returns, run destructors then park
    call    run_fini_array
.halt:
    hlt
    jmp     .halt

; ----------------------------------------------------------------------------
; _start_stack: loader pushed args on stack: [ret][api][tid]
; ----------------------------------------------------------------------------
global _start_stack
_start_stack:
    ; Move args from stack into registers
    mov     rdi, [rsp + 8]          ; api
    mov     rsi, [rsp + 16]         ; tid (use low 32 bits)
    and     rsp, -16

    ; Save API/TID into globals
    mov     [rel NOS], rdi
    mov     [rel NOS_TID], esi

    ; Run constructors
    call    run_init_array

    ; Call user's C entry
    mov     rdi, [rel NOS]
    mov     esi, dword [rel NOS_TID]
    call    init_main

    ; Run destructors then park
    call    run_fini_array
.halt2:
    hlt
    jmp     .halt2

    .section .note.GNU-stack,"",@progbits

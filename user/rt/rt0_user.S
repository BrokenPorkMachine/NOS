; ============================================================================
; user/rt/rt0_user.S — NASM syntax startup for NOS user programs
; Tries _agent_main(), else main(), else halts
; ============================================================================

BITS 64
default rel

global _start

extern _agent_main
extern main

section .text align=16

_start:
    and     rsp, -16        ; keep stack aligned

    ; Try _agent_main first
    mov     rax, _agent_main
    test    rax, rax
    jz      .try_main
    call    rax
    jmp     .done

.try_main:
    mov     rax, main
    test    rax, rax
    jz      .done
    call    rax

.done:
    hlt
    jmp     .done

section .note.GNU-stack noalloc noexec nowrite progbits

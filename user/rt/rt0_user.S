; ============================================================================
; user/rt/rt0_user.S — NASM syntax, generic startup for NOS user programs
; Calls _agent_main() if present, else main(), else halts
; ============================================================================

BITS 64
default rel

global _start

extern _agent_main
extern main

section .text

; Tell linker these are weak symbols (won’t fail if missing)
section .gnu.linkonce.t._agent_main
weak _agent_main

section .gnu.linkonce.t.main
weak main

section .text

_start:
    and     rsp, -16        ; keep stack aligned

    ; Try _agent_main first
    mov     rax, _agent_main
    test    rax, rax
    jz      .try_main
    call    _agent_main
    jmp     .done

.try_main:
    mov     rax, main
    test    rax, rax
    jz      .done
    call    main

.done:
    hlt
    jmp     .done

section .note.GNU-stack noalloc noexec nowrite progbits

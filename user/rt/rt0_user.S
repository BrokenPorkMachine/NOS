; user/rt/rt0_user.S — NASM syntax
; Generic CRT0 for NOS user programs
; Will call _agent_main() if present, else main()

BITS 64
default rel

; External but weak so linker won't fail if missing
extern _agent_main
extern main
global _start

section .text

_start:
    ; Align stack
    and     rsp, -16

    ; Try calling _agent_main if it exists
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

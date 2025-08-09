; user/rt/rt0_user.S
global _start
extern main
section .text
_start:
    xor ebp, ebp
    call main
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc

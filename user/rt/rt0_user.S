// user/rt/rt0_stackshim.S
    .text
    .globl _start
_start:
    mov   8(%rsp), %rdi     # api
    mov  16(%rsp), %rsi     # tid
    andq  $-16, %rsp
    jmp   _start_with_args

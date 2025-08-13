    .text
    .globl _start
    .type  _start, @function
_start:
    // Ensure 16-byte alignment before any call (SysV x86-64 ABI)
    andq    $-16, %rsp
    // Expect RDI = api, RSI = tid from the loader
    call    init_main
.Lpark:
    hlt
    jmp     .Lpark
    .size _start, .-_start
section .note.GNU-stack noalloc

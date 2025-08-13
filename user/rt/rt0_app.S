    .text
    .globl _start
    .type  _start, @function
    .extern main
_start:
    andq    $-16, %rsp        # Align stack
    xor     %edi, %edi        # argc = 0
    xor     %esi, %esi        # argv = NULL
    call    main
    hlt
    jmp     .
    .section .note.GNU-stack,"",@progbits

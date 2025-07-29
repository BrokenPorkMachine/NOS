global context_switch
context_switch:
    ; Arguments: rdi = &old_rsp, rsi = new_rsp
    mov [rdi], rsp
    mov rsp, rsi
    ret

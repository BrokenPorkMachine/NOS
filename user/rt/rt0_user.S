/* ============================================================================
 * user/rt/rt0_user.S  -- minimal userland CRT0 for NOS agents
 *
 * Exposes:
 *   - _start        : loader passes RDI=api, RSI=tid (SysV x86-64)
 *   - _start_stack  : loader pushed args on stack: [ret][api][tid]
 *
 * Defines the globals:
 *   - NOS     : const AgentAPI* (set from api)
 *   - NOS_TID : uint32_t        (set from tid)
 *
 * Behavior:
 *   1) Normalize stack (16B alignment).
 *   2) Save api/tid into NOS/NOS_TID.
 *   3) Run .init_array constructors if present.
 *   4) Call init_main(api, tid)  // change to agent_main if you prefer
 *   5) Run .fini_array (best-effort) if we ever return.
 *   6) Park with HLT loop.
 * ========================================================================== */

    .text
    .intel_syntax noprefix

/* --------------------------------------------------------------------------
 * Externs we call or reference
 * -------------------------------------------------------------------------- */
    .extern init_main                /* C signature: void init_main(const void*, unsigned int) */

/* Optional arrays are weak so file links even if none exist */
    .weak   __init_array_start
    .weak   __init_array_end
    .weak   __fini_array_start
    .weak   __fini_array_end

/* --------------------------------------------------------------------------
 * Globals we DEFINE here (storage for agent_abi.h externs)
 *   const AgentAPI *NOS;
 *   uint32_t        NOS_TID;
 * -------------------------------------------------------------------------- */
    .data
    .globl  NOS
    .align  8
NOS:
    .quad   0

    .globl  NOS_TID
    .align  4
NOS_TID:
    .long   0
    .long   0            /* pad to 8 bytes (keeps RIP-relative loads happy) */

/* --------------------------------------------------------------------------
 * Helpers: run .init_array and .fini_array
 * -------------------------------------------------------------------------- */
    .text
    .align  16
run_init_array:
    /* if (__init_array_start && __init_array_end) for (p=start; p<end; ++p) (*p)(); */
    lea     rdi, __init_array_start[rip]
    lea     rsi, __init_array_end[rip]
    cmp     rdi, rsi
    jae     .Linit_done
.Linit_loop:
    cmp     rdi, rsi
    jae     .Linit_done
    mov     rax, [rdi]
    add     rdi, 8
    test    rax, rax
    je      .Linit_loop
    call    rax
    jmp     .Linit_loop
.Linit_done:
    ret

run_fini_array:
    /* Run in reverse order: for (p=end; p>start;) (*--p)(); */
    lea     rdi, __fini_array_start[rip]
    lea     rsi, __fini_array_end[rip]
    cmp     rdi, rsi
    jae     .Lfini_done
.Lfini_loop:
    cmp     rdi, rsi
    jae     .Lfini_done
    sub     rsi, 8
    mov     rax, [rsi]
    test    rax, rax
    je      .Lfini_loop
    call    rax
    jmp     .Lfini_loop
.Lfini_done:
    ret

/* --------------------------------------------------------------------------
 * Entry (register ABI): _start(RDI=api, RSI=tid)
 * -------------------------------------------------------------------------- */
    .globl  _start
    .type   _start, @function
_start:
    /* Keep SysV 16-byte alignment prior to a call */
    and     rsp, -16

    /* Save API/TID into globals */
    lea     rax, NOS[rip]
    mov     [rax], rdi            /* NOS = api */
    lea     rax, NOS_TID[rip]
    mov     [rax], esi            /* NOS_TID = tid (lower 32 bits) */

    /* Run constructors if present */
    call    run_init_array

    /* Call user's C entry */
    mov     rdi, [NOS[rip]]       /* rdi = api */
    mov     esi, dword ptr [NOS_TID[rip]]  /* rsi = tid (zero-extended) */
    call    init_main

    /* If it ever returns, run destructors then park */
    call    run_fini_array

.Lpark:
    hlt
    jmp     .Lpark
    .size   _start, . - _start

/* --------------------------------------------------------------------------
 * Alternate entry (stack ABI): _start_stack()
 *   Stack layout on entry (top -> bottom):
 *      [retaddr] [api (8 bytes)] [tid (8 bytes)]
 * -------------------------------------------------------------------------- */
    .globl  _start_stack
    .type   _start_stack, @function
_start_stack:
    /* Move args from stack into registers, then tail into _start path */
    mov     rdi, [rsp + 8]        /* api */
    mov     rsi, [rsp + 16]       /* tid (use low 32 bits) */
    and     rsp, -16

    /* Save API/TID into globals */
    lea     rax, NOS[rip]
    mov     [rax], rdi
    lea     rax, NOS_TID[rip]
    mov     [rax], esi

    /* Run constructors if present */
    call    run_init_array

    /* Call user's C entry */
    mov     rdi, [NOS[rip]]
    mov     esi, dword ptr [NOS_TID[rip]]
    call    init_main

    /* Run destructors then park */
    call    run_fini_array
.Lpark2:
    hlt
    jmp     .Lpark2
    .size   _start_stack, . - _start_stack

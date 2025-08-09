#include <stdint.h>

/* ---- Syscall ABI (int 0x80) -------------------------------------
 * rax = syscall number
 * rdi, rsi, rdx, r10, r8, r9 = arg1..arg6 (we only use first three here)
 * returns: rax
 * NOTE: Match these numbers with your kernel’s dispatcher.
 */
enum {
    SYS_YIELD = 0,
    SYS_PRINT = 1,
    /* Add more as needed, e.g. SYS_EXIT = 2 */
};

static inline long sys_call3(long num, long a1, long a2, long a3) {
    register long rax __asm__("rax") = num;
    register long rdi __asm__("rdi") = a1;
    register long rsi __asm__("rsi") = a2;
    register long rdx __asm__("rdx") = a3;
    __asm__ volatile("int $0x80"
                     : "+r"(rax)
                     : "r"(rdi), "r"(rsi), "r"(rdx)
                     : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
    return rax;
}

static inline long sys_call1(long num, long a1) {
    return sys_call3(num, a1, 0, 0);
}

static inline long sys_yield(void) {
    return sys_call1(SYS_YIELD, 0);
}

static inline long sys_print(const char *s) {
    /* If your kernel expects (ptr, len), pass len too. */
    return sys_call3(SYS_PRINT, (long)s, 0, 0);
}

/* Runs in ring3 */
void user_task(void) {
    static const char message[] = "U-task\n";

    /* Optional: ensure stack alignment if you call other C funcs here.
       enter_user_mode should give you a 16-byte aligned RSP. */

    (void)sys_print(message);

    for (;;) {
        (void)sys_yield();
        /* A tiny polite spin so we’re not hammering the kernel too hard
           if SYS_YIELD is a no-op in some states. */
        __asm__ volatile("pause");
    }

    /* If you add an exit syscall, you can do:
       (void)sys_call1(SYS_EXIT, 0);
    */
}

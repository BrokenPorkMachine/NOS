#include <stdint.h>

/* Runs in ring3 */
void __attribute__((naked)) user_task(void) {
    asm volatile(
        "lea message(%rip), %rdi\n"
        "mov $1, %rax\n"   /* SYS_PRINT */
        "int $0x80\n"
        "mov $0, %rax\n"   /* SYS_YIELD */
        "int $0x80\n"
    ".hang:\n"
        "hlt\n"
        "jmp .hang\n"
    "message: .asciz \"U-task\\n\"\n"
    : : : "rdi", "rax"
    );
}

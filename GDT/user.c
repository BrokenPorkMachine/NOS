#include <stdint.h>

void __attribute__((naked)) user_task(void) {
    asm volatile(
        "lea message(%%rip), %%rdi\n"
        "mov $1, %%rax\n"  // SYS_WRITE_VGA
        "int $0x80\n"
        "mov $0, %%rax\n"  // SYS_YIELD
        "int $0x80\n"
        "1: hlt\n"
        "jmp 1b\n"
        "message: .asciz \"U-task\n\""
        : : : "rdi", "rax"
    );
}

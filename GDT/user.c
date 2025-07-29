#include <stdint.h>

void __attribute__((naked)) user_task(void) {
    asm volatile(
        "mov $'U', %%al\n"
        "mov $0xB8000, %%rbx\n"
        "mov %%al, (%%rbx)\n"
        "mov $0, %%rax\n"    // syscall: yield
        "int $0x80\n"       // Trap back to kernel
        "1: hlt\n"
        "jmp 1b\n"
        :::"rbx", "al"
    );
}

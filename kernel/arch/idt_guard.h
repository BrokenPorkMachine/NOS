// kernel/arch/idt_guard.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the IDT guard once:
// - Clones the current IDT into a writable kernel buffer
// - If vector 6 (#UD) or 13 (#GP) point into legacy low memory (0xA0000..0xFFFFF),
//   they are replaced with safe 64-bit stubs that simply iretq (GP also drops error code).
// - Loads the cloned IDT with lidt.
// - Idempotent: safe to call multiple times.
void idt_guard_init_once(void);

// Optionally replace our guard stubs later with real kernel handlers.
// You can pass NULL for either to keep the current handler entry as-is.
void idt_guard_install_real_handlers(void (*ud_handler)(void), void (*gp_handler)(void));

// Returns non-zero if the active IDT is the cloned guard table.
int idt_guard_is_active(void);

#ifdef __cplusplus
}
#endif

#pragma once
#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All handlers receive a pointer to the CPU/context frame the stubs built.
 * Stubs preserve all GPRs and pass RDI = (struct isr_context *).
 *
 * Fatal handlers (default, #GP when unhandled) do not return.
 * Others may fix up state and return to the interrupted context.
 */

/* ----- Fatal / generic ----- */
void isr_default_handler(struct isr_context *ctx) __attribute__((noreturn));
void isr_gpf_handler(struct isr_context *ctx) __attribute__((noreturn));

/* ----- Exceptions / syscalls / IRQs ----- */
void isr_page_fault_handler(struct isr_context *ctx);   /* may return if handled */
void isr_syscall_handler(struct isr_context *ctx);      /* int 0x80 compat */

void isr_timer_handler(struct isr_context *ctx);        /* IRQ0 or APIC timer */
void isr_keyboard_handler(struct isr_context *ctx);     /* IRQ1 (if used) */
void isr_mouse_handler(struct isr_context *ctx);        /* IRQ12 (if used) */
void isr_ipi_handler(struct isr_context *ctx);          /* IPI vector */

/* Optional: if you map a spurious APIC vector, you can hook this */
void isr_spurious_handler(struct isr_context *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

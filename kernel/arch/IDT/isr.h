#pragma once
#include "context.h"

// All handlers receive a pointer to context
void isr_default_handler(struct isr_context *ctx) __attribute__((noreturn));
void isr_timer_handler(struct isr_context *ctx);
void isr_syscall_handler(struct isr_context *ctx);
void isr_keyboard_handler(struct isr_context *ctx);
void isr_mouse_handler(struct isr_context *ctx);
void isr_page_fault_handler(struct isr_context *ctx);
void isr_gpf_handler(struct isr_context *ctx);
void isr_ipi_handler(struct isr_context *ctx);

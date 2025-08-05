#include "context.h"
#include "../../drivers/IO/serial.h"
#include "../../VM/cow.h"
#include "../CPU/lapic.h"

void isr_default_handler(struct isr_context *ctx) __attribute__((noreturn));
void isr_timer_handler(struct isr_context *ctx);
void isr_page_fault_handler(struct isr_context *ctx);
void isr_ipi_handler(struct isr_context *ctx);

void isr_default_handler(struct isr_context *ctx) {
    // Print vector number and registers
    serial_printf("\n[FAULT] Unhandled interrupt: %lu\n", ctx->int_no);

    // Print all general purpose registers
    serial_printf("RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    serial_printf("RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n", ctx->rsi, ctx->rdi, ctx->rbp, (uint64_t)ctx);
    serial_printf("R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n", ctx->r8, ctx->r9, ctx->r10, ctx->r11);
    serial_printf("R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n", ctx->r12, ctx->r13, ctx->r14, ctx->r15);

    // Print stack frame info
    serial_printf("RIP=%016lx CS=%04lx RFLAGS=%016lx\n", ctx->rip, ctx->cs, ctx->rflags);

    // Print error code if available (for faults)
    serial_printf("Error Code: 0x%016lx\n", ctx->error_code);

    // Optionally: Print a mini stack trace (just show top 4 qwords of stack)
    uint64_t *sp = (uint64_t *)(ctx->user_rsp ? ctx->user_rsp : (uint64_t)ctx + sizeof(*ctx));
    serial_printf("Stack (top 4): ");
    for (int i = 0; i < 4; ++i) {
        serial_printf("%016lx ", sp[i]);
    }
    serial_puts("\n");

    // Print to VGA as well (just first 80 chars of message)
    volatile uint16_t *vga = (uint16_t *)(0xB8000) + 80; // line 2
    const char *msg = "FAULT! IRQ: ";
    for (int i = 0; msg[i]; ++i)
        vga[i] = msg[i] | (0x47 << 8);
    vga[12] = '0' + (ctx->int_no % 10) | (0x47 << 8);

    while (1)
        __asm__ volatile ("hlt");
}

void isr_timer_handler(struct isr_context *ctx) {
    static int ticks = 0;
    ++ticks;
    volatile uint16_t *vga = (uint16_t *)(0xB8000) + 80; // line 2
    vga[0] = ('0' + (ticks % 10)) | (0x2F << 8);
    (void)ctx; // Not used
}

void isr_page_fault_handler(struct isr_context *ctx) {
    serial_printf("[FAULT] Page fault at 0x%lx, error=0x%lx\n", ctx->cr2, ctx->error_code);
    handle_page_fault(ctx->error_code, ctx->cr2);
}

void isr_ipi_handler(struct isr_context *ctx) {
    lapic_eoi();
    (void)ctx;
}

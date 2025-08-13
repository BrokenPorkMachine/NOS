#include "isr.h"
#include "../../../nosm/drivers/IO/serial.h"
#include "../../VM/paging_adv.h"
#include "../CPU/lapic.h"
#include "../CPU/smp.h"
#include <stddef.h>
#include <stdint.h>

/* Provided by the scheduler to allow preemptive multitasking from the
 * timer interrupt context. */
extern uint64_t schedule_from_isr(uint64_t *old_rsp);

/* ---- Helpers ---------------------------------------------------- */

static inline int is_irq_vector(uint64_t vec) {
    /* 0..31 are CPU exceptions; 32+ are (typically) IRQs after PIC/APIC remap */
    return (vec >= 32);
}

static inline void apic_eoi_if_needed(uint64_t vec) {
    if (is_irq_vector(vec)) {
        lapic_eoi();
    }
}

/* Very small, safe-ish backtrace using frame pointers (RBP chain).
   We cap the depth and guard with a simple canonical address check. */
static void backtrace_rbp(uint64_t rbp, int max_frames) {
    serial_puts("Backtrace:");
    int frames = 0;
    while (rbp && frames < max_frames) {
        uint64_t *frame = (uint64_t *)rbp;
        uint64_t next_rbp = frame[0];
        uint64_t ret_addr = frame[1];
        serial_printf("  [%d] rip=%016lx\n", frames, ret_addr);

        /* basic canonical addr check to avoid wandering into la-la land */
        if ((next_rbp & 0xFFFF800000000000ULL) &&
            ((next_rbp & 0xFFFF800000000000ULL) != 0xFFFF800000000000ULL))
            break;

        rbp = next_rbp;
        ++frames;
    }
    if (frames == 0) serial_puts("  <empty>\n");
}

/* Decode page-fault error code per SDM Vol.3: P/WR/US/RSVD/IFETCH/PK/SS */
static void dump_pf_bits(uint64_t err) {
    serial_printf("  present=%d write=%d user=%d rsvd=%d exec=%d pk=%d ss=%d\n",
                  (int)(err & 1),
                  (int)((err >> 1) & 1),
                  (int)((err >> 2) & 1),
                  (int)((err >> 3) & 1),
                  (int)((err >> 4) & 1),
                  (int)((err >> 5) & 1),
                  (int)((err >> 6) & 1));
}

/* Safer tiny VGA poke (row 2, cols 0..15) */
static void vga_write_line2(const char *s, uint8_t attr) {
    volatile uint16_t *v = (uint16_t *)(uintptr_t)0xB8000 + 80 * 2;
    for (int i = 0; s[i] && i < 80; ++i) v[i] = ((uint16_t)s[i]) | ((uint16_t)attr << 8);
}

/* Dump register state for debugging faults */
static void dump_context(struct isr_context *ctx) {
    serial_printf("CPU=%u APIC=%u  VEC=%lu\n", smp_cpu_id(), lapic_get_id(), ctx->int_no);

    serial_printf("RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n",
                  ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    serial_printf("RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n",
                  ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp);
    serial_printf("R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n",
                  ctx->r8, ctx->r9, ctx->r10, ctx->r11);
    serial_printf("R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n",
                  ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    serial_printf("RIP=%016lx CS=%04lx RFLAGS=%016lx SS=%04lx\n",
                  ctx->rip, ctx->cs, ctx->rflags, ctx->ss);
    serial_printf("ERR=%016lx CR2=%016lx\n", ctx->error_code, ctx->cr2);

    /* Show a few stack qwords (current RSP) */
    uint64_t *sp = (uint64_t *)(uintptr_t)(ctx->rsp);
    serial_printf("Stack[0..3]: ");
    for (int i = 0; i < 4; ++i) serial_printf("%016lx ", sp[i]);
    serial_puts("\n");
}

/* ---- Handlers --------------------------------------------------- */

void isr_default_handler(struct isr_context *ctx) __attribute__((noreturn));
void isr_default_handler(struct isr_context *ctx) {
    /* Ack any unexpected IRQ so we don't livelock the APIC. */
    apic_eoi_if_needed(ctx->int_no);

    serial_printf("\n[FAULT] Unhandled interrupt vector: %lu\n", ctx->int_no);
    dump_context(ctx);
    backtrace_rbp(ctx->rbp, 16);

    /* On-screen hint */
    char msg[32];
    /* quick itoa for the last digit just to keep your old style */
    int d = (int)(ctx->int_no % 10);
    msg[0] = 'F'; msg[1] = 'A'; msg[2] = 'U'; msg[3] = 'L'; msg[4] = 'T';
    msg[5] = '!'; msg[6] = ' '; msg[7] = 'V'; msg[8] = 'E'; msg[9] = 'C';
    msg[10] = ':'; msg[11] = ' '; msg[12] = (char)('0' + d); msg[13] = 0;
    vga_write_line2(msg, 0x47);

    /* Re-enable IF before halting so NMIs/SMIs can still get through */
    __asm__ volatile ("sti");
    for (;;) __asm__ volatile ("hlt");
}

void isr_gpf_handler(struct isr_context *ctx) __attribute__((noreturn));
void isr_gpf_handler(struct isr_context *ctx) {
    /* #GP is a fault, not an IRQ; no EOI */
    serial_puts("\n[FAULT] General Protection Fault (#GP)\n");
    dump_context(ctx);
    backtrace_rbp(ctx->rbp, 16);

    __asm__ volatile ("sti");
    for (;;) __asm__ volatile ("hlt");
}

void isr_timer_handler(struct isr_context *ctx) {
    static uint64_t ticks;
    ++ticks;

    /* Tiny on-screen heartbeat */
    volatile uint16_t *vga = (uint16_t *)(uintptr_t)0xB8000 + 80 * 0; /* first row */
    vga[0] = ((uint16_t)('0' + (ticks % 10))) | ((uint16_t)0x2F << 8);

    /* Preemptive scheduling: switch to the next runnable thread if needed. */
    ctx->rsp = schedule_from_isr((uint64_t *)ctx->rsp);

    (void)ctx;
    apic_eoi_if_needed(32); /* PIT/APIC timer typically at vector 32 */
}

void isr_page_fault_handler(struct isr_context *ctx) {
    serial_puts("[FAULT] Page Fault (#PF)\n");
    dump_context(ctx);
    dump_pf_bits(ctx->error_code);

    /* Let the VM layer try to handle it; implementation halts if unrecoverable */
    paging_handle_fault(ctx->error_code, ctx->cr2, smp_cpu_id());
    return;
}

void isr_ud_handler(struct isr_context *ctx) {
    serial_printf("[#UD] rip=%016lx cs=%04lx rfl=%016lx\n",
                  ctx->rip, ctx->cs, ctx->rflags);
    const uint8_t *p = (const uint8_t *)ctx->rip;
    serial_printf("Bytes: ");
    for (int i = 0; i < 16; ++i) serial_printf("%02x ", p[i]);
    serial_puts("\n");
    for (;;) __asm__ volatile("hlt");
}

void isr_ipi_handler(struct isr_context *ctx) {
    /* IPI is an IRQ -> always EOI */
    apic_eoi_if_needed(ctx->int_no);
    (void)ctx;
}

/* Optional: “spurious interrupt” vector handler, if you mapped one (e.g., 255) */
void isr_spurious_handler(struct isr_context *ctx) {
    /* For APIC spurious vector: read EOI is not required (per Intel),
       but harmless to write. We'll skip it to be pedantic. */
    serial_printf("[WARN] Spurious interrupt on CPU %u (vec=%lu)\n",
                  smp_cpu_id(), ctx->int_no);
    (void)ctx;
}

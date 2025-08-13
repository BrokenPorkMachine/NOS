// nitros_arch_sched.c
// Drop-in arch + minimal scheduler glue for NitrOS (x86-64, no LDT).
// - Flat 64-bit GDT (kernel/user code+data) + 64-bit TSS
// - Safe user-mode iretq (CS=0x1B, SS=0x23) — TI=0 only
// - Thread spawn initializes CS/SS explicitly
// - Restore path validates selectors; logs if TI=1 or unexpected
//
// Build: add this file to your kernel; provide log_error/log_info, alloc_* APIs, and linker symbol _kernel_stack_top.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "arch_x86_64/gdt_tss.h"

extern uint8_t _kernel_stack_top[];

/* ------------------ Minimal logging hooks (replace with your own) ------------- */
__attribute__((weak)) void log_error(const char *fmt, ...) { (void)fmt; }
__attribute__((weak)) void log_info (const char *fmt, ...) { (void)fmt; }

struct cpu_context {
    /* Callee-saved first keeps ABI happy if you ever use setjmp/longjmp style */
    uint64_t r15, r14, r13, r12, rbx, rbp;
    /* Minimal iretq frame + convenience */
    uint64_t rip, cs, rflags, rsp, ss;
};

struct thread {
    struct cpu_context cpu_ctx;
    int priority;
    int user_mode; // 0 = kernel, 1 = user
    /* ... add state fields as needed ... */
};

size_t thread_struct_size = sizeof(struct thread);

/* ------------- Externals you typically already have in your kernel ----------- */
extern void   *alloc_stack(size_t size, int user_mode);
extern void   *alloc_thread_struct(void);
extern void    scheduler_enqueue(struct thread *t);
extern void    scheduler_pick_and_run(void); // you can keep yours; we also include a simple example below
extern uint8_t _kernel_stack_top[];          // from linker script

/* ---------------- Safe user-mode transition (uses iretq) -------------------- */
__attribute__((noreturn)) static void switch_to_user(void *entry, void *user_stack_top)
{
    // System V ABI: entry in RDI, stack in RSI if called from C; we’ll use the actual args passed here.
    // Build an iretq frame with CS=0x1B, SS=0x23, RFLAGS.IF=1. Set DS/ES/FS/GS to 0x23 too.
    __asm__ __volatile__ (
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "pushq $0x23\n\t"          // SS
        "pushq %0\n\t"             // RSP
        "pushq $0x202\n\t"         // RFLAGS (IF=1)
        "pushq $0x1B\n\t"          // CS
        "pushq %1\n\t"             // RIP
        "iretq\n\t"
        :
        : "r"(user_stack_top), "r"(entry)
        : "rax", "memory"
    );
    __builtin_unreachable();
}

/* ---------------------- Thread spawn helpers (safe selectors) --------------- */
static inline void init_user_thread_ctx(struct cpu_context *ctx, void *entry, void *stack_top)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->rip    = (uint64_t)entry;
    ctx->cs     = GDT_USER_CODE;
    ctx->rflags = 0x202;              // IF=1
    ctx->rsp    = (uint64_t)stack_top;
    ctx->ss     = GDT_USER_DATA;
}

static inline void init_kernel_thread_ctx(struct cpu_context *ctx, void *entry, void *stack_top)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->rip    = (uint64_t)entry;
    ctx->cs     = GDT_KERNEL_CODE;
    ctx->rflags = 0x202;              // IF=1
    ctx->rsp    = (uint64_t)stack_top;
    ctx->ss     = GDT_KERNEL_DATA;
}

/* ---------------------- Defensive selector validation ----------------------- */
static inline int is_selector_ti1(uint16_t sel) { return (sel & 0x4) != 0; }
static inline int is_ring3(uint16_t sel)        { return (sel & 0x3) == 3; }

static inline int is_valid_selector_pair(uint16_t cs, uint16_t ss)
{
    if (is_selector_ti1(cs) || is_selector_ti1(ss)) return 0; // LDT not allowed
    if (is_ring3(cs)) {
        return (cs == GDT_USER_CODE) && (ss == GDT_USER_DATA);
    } else {
        return (cs == GDT_KERNEL_CODE) && (ss == GDT_KERNEL_DATA);
    }
}

/* ----------------------- Restore + enter (first run) ------------------------ */
static void restore_thread_context_first_run(struct cpu_context *ctx)
{
    uint16_t cs = (uint16_t)ctx->cs, ss = (uint16_t)ctx->ss;

    if (!is_valid_selector_pair(cs, ss)) {
        log_error("[sched] Invalid selector pair CS=0x%04x SS=0x%04x (TI bits? ring mismatch?). Forcing safe defaults.",
                  cs, ss);
        if (is_ring3(cs)) {
            ctx->cs = GDT_USER_CODE;
            ctx->ss = GDT_USER_DATA;
        } else {
            ctx->cs = GDT_KERNEL_CODE;
            ctx->ss = GDT_KERNEL_DATA;
        }
    }

    if (is_ring3((uint16_t)ctx->cs)) {
        switch_to_user((void*)ctx->rip, (void*)ctx->rsp);
    } else {
        // Kernel "entry" — just call it directly on this CPU with the prepared stack.
        // Switch RSP then do an indirect jump.
        __asm__ __volatile__ (
            "mov %0, %%rsp\n\t"
            "jmp *%1\n\t"
            :
            : "r"(ctx->rsp), "r"(ctx->rip)
            : "memory"
        );
        __builtin_unreachable();
    }
}

/* ----------------------------- Thread API ---------------------------------- */
struct thread *thread_spawn(void (*entry)(void), size_t stack_size, int priority, int user_mode)
{
    struct thread *t = (struct thread*)alloc_thread_struct();
    if (!t) {
        log_error("[thread] alloc_thread_struct failed");
        return NULL;
    }

    void *stack_top = alloc_stack(stack_size, user_mode);
    if (!stack_top) {
        log_error("[thread] alloc_stack failed");
        return NULL;
    }

    t->priority  = priority;
    t->user_mode = user_mode;

    if (user_mode)  init_user_thread_ctx(&t->cpu_ctx, (void*)entry, stack_top);
    else            init_kernel_thread_ctx(&t->cpu_ctx, (void*)entry, stack_top);

    scheduler_enqueue(t);
    return t;
}

/* -------------------------- Minimal scheduler demo -------------------------- */
/* If you already have a scheduler, keep it. This shows how to enter the first user thread safely. */

#ifndef NITROS_HAS_SCHEDULER_PICK_AND_RUN
// Simple placeholder queue; replace with your own.
#ifndef MAX_RUNQ
#define MAX_RUNQ 32
#endif

static struct thread *runq[MAX_RUNQ];
static size_t rq_head = 0, rq_tail = 0;

__attribute__((weak)) void scheduler_enqueue(struct thread *t)
{
    size_t nxt = (rq_tail + 1) % MAX_RUNQ;
    if (nxt == rq_head) { log_error("[sched] runqueue full"); return; }
    runq[rq_tail] = t; rq_tail = nxt;
}

static struct thread *scheduler_dequeue(void)
{
    if (rq_head == rq_tail) return NULL;
    struct thread *t = runq[rq_head];
    rq_head = (rq_head + 1) % MAX_RUNQ;
    return t;
}

__attribute__((weak)) void scheduler_pick_and_run(void)
{
    struct thread *t = scheduler_dequeue();
    if (!t) { log_error("[sched] empty runqueue"); for(;;) __asm__ __volatile__("hlt"); }
    restore_thread_context_first_run(&t->cpu_ctx);
}
#endif /* NITROS_HAS_SCHEDULER_PICK_AND_RUN */

/* --------------------------- Arch boot entry glue --------------------------- */
/* Call this from your n2_main() right after IDT init and before spawning threads. */
void arch_early_init_and_start(void (*spawn_all)(void))
{
    gdt_tss_init(_kernel_stack_top);
    // Your code should set up IDT earlier; if not, do it before this call.
    // After GDT/TSS is live, let the caller spawn kernel/user threads:
    if (spawn_all) spawn_all();

#ifndef NITROS_HAS_SCHEDULER_PICK_AND_RUN
    // Enter the very first thread (user or kernel) in a safe way.
    scheduler_pick_and_run();
#endif
}

/* -------------------------- Example “spawn_all” hook ------------------------ */
/* You likely already have logic that creates core kernel services and user init.
   Keep that and just call arch_early_init_and_start(spawn_all). Example: */

#if 0
extern void core_service_1(void);
extern void core_service_2(void);
extern void user_init(void); // entry in your init.mo2

void spawn_all_example(void) {
    thread_spawn(core_service_1, 1<<14, /*prio*/10, /*user*/0);
    thread_spawn(core_service_2, 1<<14,  5,         0);
    thread_spawn((void(*)(void))user_init, 1<<14, 20, 1);
}

int kmain(void) {
    // idt_init();  // do this before
    arch_early_init_and_start(spawn_all_example);
    for (;;) __asm__ __volatile__("hlt");
}
#endif

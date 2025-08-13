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

/* -------- Public selectors (keep in sync with your IDT/syscall paths) -------- */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x1B
#define GDT_USER_DATA   0x23
#define GDT_TSS         0x28  // TSS selector (occupies two GDT slots at indices 5+6)

/* ------------------ Minimal logging hooks (replace with your own) ------------- */
__attribute__((weak)) void log_error(const char *fmt, ...) { (void)fmt; }
__attribute__((weak)) void log_info (const char *fmt, ...) { (void)fmt; }

/* --------------------------- Core structs ------------------------------------ */
struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) tss64 {
    uint32_t _rsvd0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t _rsvd1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t _rsvd2;
    uint16_t _rsvd3;
    uint16_t iomap_base;
};

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

/* ------------- Externals you typically already have in your kernel ----------- */
extern void   *alloc_stack(size_t size, int user_mode);
extern void   *alloc_thread_struct(void);
extern void    scheduler_enqueue(struct thread *t);
extern void    scheduler_pick_and_run(void); // you can keep yours; we also include a simple example below
extern uint8_t _kernel_stack_top[];          // from linker script

/* ----------------------- Arch globals (GDT/TSS) ------------------------------ */
static uint64_t gdt[7];       // 0..4 segments + 5/6 TSS (two slots)
static struct tss64 tss;
static struct gdt_ptr gdtp;

/* ----------------------- Helper: set 64-bit TSS descriptor ------------------- */
static void set_tss_descriptor(uint64_t *table, int slot, struct tss64 *t)
{
    // slot must point to the first of two consecutive entries
    uint64_t base  = (uint64_t)t;
    uint64_t limit = sizeof(*t) - 1;

    uint64_t low =
        ((limit & 0xFFFFULL)) |
        ((base  & 0xFFFFFFULL) << 16) |
        (0x89ULL << 40) |                 // type=0x9 (Avail 64-bit TSS), present=1, DPL=0, G=0
        ((limit & 0xF0000ULL) << 48) |
        ((base  & 0xFF000000ULL) << 32);

    uint64_t high = (base >> 32) & 0xFFFFFFFFULL; // upper 32 bits of base
    table[slot]     = low;
    table[slot + 1] = high;
}

/* -------------------------- lgdt + CS reload -------------------------------- */
static inline void lgdt_and_reload_segments(const struct gdt_ptr *gp)
{
    // LGDT + reload data segs + far return to reload CS
    __asm__ __volatile__ (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        /* Far return to set CS = 0x08 */
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "r"(gp)
        : "rax", "memory"
    );
}

/* --------------------------------- ltr -------------------------------------- */
static inline void ltr(uint16_t sel)
{
    __asm__ __volatile__("ltr %0" : : "r"(sel) : "memory");
}

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

/* -------------------------- Public arch init API ---------------------------- */
void arch_gdt_tss_init(void)
{
    // 0: null
    gdt[0] = 0x0000000000000000ULL;
    // 1: kernel code 64-bit
    gdt[1] = 0x00AF9A000000FFFFULL;
    // 2: kernel data
    gdt[2] = 0x00AF92000000FFFFULL;
    // 3: user code
    gdt[3] = 0x00AFFA000000FFFFULL;
    // 4: user data
    gdt[4] = 0x00AFF2000000FFFFULL;
    // 5/6: TSS (two entries)
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = (uint64_t)_kernel_stack_top;   // kernel RSP0 used on privilege transitions
    tss.iomap_base = sizeof(tss);             // no IO bitmap
    set_tss_descriptor(gdt, 5, &tss);

    gdtp.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtp.base  = (uint64_t)gdt;

    lgdt_and_reload_segments(&gdtp);
    ltr(GDT_TSS);
    log_info("[arch] GDT/TSS initialized (no LDT)");
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
    arch_gdt_tss_init();
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

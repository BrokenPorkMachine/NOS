// kernel/arch/idt_guard.c
#include "idt_guard.h"

#include <string.h>
#include <stdint.h>

#ifdef IDT_GUARD_DEBUG
extern int printf(const char *fmt, ...);
#define DPRINT(...) printf(__VA_ARGS__)
#else
#define DPRINT(...) do{}while(0)
#endif

struct __attribute__((packed)) idt_desc64 { uint16_t limit; uint64_t base; };
struct __attribute__((packed)) idt_entry64 {
    uint16_t off_lo;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t off_mid;
    uint32_t off_hi;
    uint32_t zero;
};

static inline uint64_t make_gate_offset(const volatile struct idt_entry64* e){
    return ((uint64_t)e->off_hi<<32) | ((uint64_t)e->off_mid<<16) | e->off_lo;
}
static inline void fill_gate(struct idt_entry64* e, uint64_t handler, uint16_t cs){
    e->off_lo   = (uint16_t)(handler & 0xFFFF);
    e->sel      = cs;
    e->ist      = 0;        // no IST
    e->type_attr= 0x8E;     // present, DPL=0, 64-bit interrupt gate
    e->off_mid  = (uint16_t)((handler>>16)&0xFFFF);
    e->off_hi   = (uint32_t)((handler>>32)&0xFFFFFFFF);
    e->zero     = 0;
}

/* Guard stubs */
__attribute__((naked)) static void ud_guard_stub(void){
    __asm__ __volatile__(
        "push %rax; push %rcx; push %rdx; push %rsi; push %rdi; push %r8; push %r9; push %r10; push %r11;\n\t"
        "pop %r11; pop %r10; pop %r9; pop %r8; pop %rdi; pop %rsi; pop %rdx; pop %rcx; pop %rax;\n\t"
        "iretq\n\t"
    );
}
__attribute__((naked)) static void gp_guard_stub(void){
    __asm__ __volatile__(
        "push %rax; push %rcx; push %rdx; push %rsi; push %rdi; push %r8; push %r9; push %r10; push %r11;\n\t"
        "pop %r11; pop %r10; pop %r9; pop %r8; pop %rdi; pop %rsi; pop %rdx; pop %rcx; pop %rax;\n\t"
        "add $8, %rsp\n\t"   /* drop error code pushed by CPU for #GP */
        "iretq\n\t"
    );
}

/* Writable cloned IDT (max 4 KiB) */
static __attribute__((aligned(16))) uint8_t g_idt_clone[4096];
static struct idt_desc64 g_idtr_clone;
static int g_guard_active = 0;

int idt_guard_is_active(void){
    return g_guard_active;
}

static void idt_guard_patch_entries(struct idt_entry64* idt, uint16_t limit){
    uint16_t cs; __asm__ volatile("mov %%cs,%0":"=r"(cs));
    if(limit + 1 >= (6+1)*sizeof(struct idt_entry64)){
        struct idt_entry64* v6 = &idt[6];
        uint64_t off6 = make_gate_offset(v6);
        if(off6>=0x00000000000A0000ULL && off6<=0x00000000000FFFFFULL){
            DPRINT("[idt-guard] patch v6 (#UD) from %016llx\n", (unsigned long long)off6);
            fill_gate(v6, (uint64_t)(uintptr_t)&ud_guard_stub, cs);
        }
    }
    if(limit + 1 >= (13+1)*sizeof(struct idt_entry64)){
        struct idt_entry64* v13 = &idt[13];
        uint64_t off13 = make_gate_offset(v13);
        if(off13>=0x00000000000A0000ULL && off13<=0x00000000000FFFFFULL){
            DPRINT("[idt-guard] patch v13 (#GP) from %016llx\n", (unsigned long long)off13);
            fill_gate(v13, (uint64_t)(uintptr_t)&gp_guard_stub, cs);
        }
    }
}

void idt_guard_init_once(void){
    if(g_guard_active) return;

    struct idt_desc64 idtr = {0};
    __asm__ volatile("sidt %0" : "=m"(idtr));
    if(!idtr.base) return;
    uint16_t limit = idtr.limit;
    if(limit >= sizeof(g_idt_clone)) limit = sizeof(g_idt_clone) - 1;

    /* Clone current IDT into writable buffer */
    memcpy(g_idt_clone, (const void*)(uintptr_t)idtr.base, (size_t)limit + 1);
    struct idt_entry64* idt = (struct idt_entry64*)g_idt_clone;

    /* Patch suspicious entries (low-mem handlers) */
    idt_guard_patch_entries(idt, limit);

    /* Load cloned IDT */
    g_idtr_clone.base  = (uint64_t)(uintptr_t)g_idt_clone;
    g_idtr_clone.limit = limit;
    __asm__ volatile("lidt %0" :: "m"(g_idtr_clone));
    g_guard_active = 1;

    DPRINT("[idt-guard] active base=%016llx limit=%04x\n",
           (unsigned long long)g_idtr_clone.base, (unsigned)g_idtr_clone.limit);
}

void idt_guard_install_real_handlers(void (*ud_handler)(void), void (*gp_handler)(void)){
    if(!g_guard_active) return;
    struct idt_entry64* idt = (struct idt_entry64*)(uintptr_t)g_idtr_clone.base;
    uint16_t limit = g_idtr_clone.limit;
    uint16_t cs; __asm__ volatile("mov %%cs,%0":"=r"(cs));

    if(ud_handler && limit + 1 >= (6+1)*sizeof(struct idt_entry64)){
        fill_gate(&idt[6], (uint64_t)(uintptr_t)ud_handler, cs);
        DPRINT("[idt-guard] installed real #UD=%p\n", ud_handler);
    }
    if(gp_handler && limit + 1 >= (13+1)*sizeof(struct idt_entry64)){
        fill_gate(&idt[13], (uint64_t)(uintptr_t)gp_handler, cs);
        DPRINT("[idt-guard] installed real #GP=%p\n", gp_handler);
    }
}

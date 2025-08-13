#include <stdint.h>
#include "../arch/GDT/gdt.h"

extern void context_switch_asm(uint64_t *old_rsp, uint64_t new_rsp);

static inline uint16_t read_cs(void){ uint16_t v; __asm__ volatile("mov %%cs,%0":"=r"(v)); return v; }
static inline uint16_t read_ds(void){ uint16_t v; __asm__ volatile("mov %%ds,%0":"=r"(v)); return v; }
static inline uint16_t read_es(void){ uint16_t v; __asm__ volatile("mov %%es,%0":"=r"(v)); return v; }
static inline uint16_t read_fs(void){ uint16_t v; __asm__ volatile("mov %%fs,%0":"=r"(v)); return v; }
static inline uint16_t read_gs(void){ uint16_t v; __asm__ volatile("mov %%gs,%0":"=r"(v)); return v; }
static inline uint16_t read_ss(void){ uint16_t v; __asm__ volatile("mov %%ss,%0":"=r"(v)); return v; }

#define CHECK_SEG(reg, stage) assert_selector_gdt(read_##reg(), stage " " #reg)

void context_switch(uint64_t *old_rsp, uint64_t new_rsp) {
    CHECK_SEG(cs, "context_switch entry");
    CHECK_SEG(ds, "context_switch entry");
    CHECK_SEG(es, "context_switch entry");
    CHECK_SEG(fs, "context_switch entry");
    CHECK_SEG(gs, "context_switch entry");
    CHECK_SEG(ss, "context_switch entry");

    context_switch_asm(old_rsp, new_rsp);

    CHECK_SEG(cs, "context_switch exit");
    CHECK_SEG(ds, "context_switch exit");
    CHECK_SEG(es, "context_switch exit");
    CHECK_SEG(fs, "context_switch exit");
    CHECK_SEG(gs, "context_switch exit");
    CHECK_SEG(ss, "context_switch exit");
}

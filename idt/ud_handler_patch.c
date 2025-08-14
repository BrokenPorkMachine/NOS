// ============================================================================
// File: idt/ud_handler_patch.c  (optional diagnostics drop-in)
// Purpose: Better #UD dump to confirm bogus RIPs
// ============================================================================
#include <stddef.h>
#include <stdint.h>

// Provide these from your kernel
extern int safe_copy_from_user(void* dst, const void* src, size_t n);
extern void hexdump(const char* tag, const void* data, size_t n);
extern void log(const char* fmt, ...);

typedef struct {
    uint64_t rip, cs, rflags, rsp, ss;
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t err;
} regs_t;

static void dump_regs(const regs_t *r)
{
    log("RIP=%016llx CS=%04llx RFLAGS=%016llx", (unsigned long long)r->rip,
        (unsigned long long)r->cs, (unsigned long long)r->rflags);
    log("RSP=%016llx SS=%04llx ERR=%llx", (unsigned long long)r->rsp,
        (unsigned long long)r->ss, (unsigned long long)r->err);
    log("RAX=%016llx RBX=%016llx RCX=%016llx RDX=%016llx",
        (unsigned long long)r->rax, (unsigned long long)r->rbx,
        (unsigned long long)r->rcx, (unsigned long long)r->rdx);
    log("RSI=%016llx RDI=%016llx RBP=%016llx",
        (unsigned long long)r->rsi, (unsigned long long)r->rdi,
        (unsigned long long)r->rbp);
    log("R8=%016llx R9=%016llx R10=%016llx R11=%016llx",
        (unsigned long long)r->r8, (unsigned long long)r->r9,
        (unsigned long long)r->r10, (unsigned long long)r->r11);
    log("R12=%016llx R13=%016llx R14=%016llx R15=%016llx",
        (unsigned long long)r->r12, (unsigned long long)r->r13,
        (unsigned long long)r->r14, (unsigned long long)r->r15);
}

void isr_ud(const regs_t* r){
    uint8_t b[16];
    if (safe_copy_from_user(b, (const void*)r->rip, sizeof b)==0) {
        hexdump("ud_bytes", b, sizeof b);
    } else {
        log("ud_bytes: <unmapped @ %p>", (void*)r->rip);
    }
    dump_regs(r);
}

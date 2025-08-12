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

typedef struct { uint64_t rip; /* ... */ } regs_t;

void isr_ud(const regs_t* r){
    uint8_t b[16];
    if (safe_copy_from_user(b, (const void*)r->rip, sizeof b)==0) {
        hexdump("ud_bytes", b, sizeof b);
    } else {
        log("ud_bytes: <unmapped @ %p>", (void*)r->rip);
    }
    // ... existing register dump ...
}

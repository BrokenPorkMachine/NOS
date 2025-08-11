#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Clone current IDT, replace any low-memory (legacy) handlers with safe 64-bit stubs,
// and LIDT the clone. Idempotent and safe to call multiple times.
void idt_guard_init_once(void);

// Optional: force install "real" handlers later (keeps the cloned IDT base).
// If you don't have final handlers yet, you can skip calling this.
void idt_guard_install_real_handlers(void (*ud_noerr)(void*),
                                     void (*gp_err)(void*, uint64_t));

#ifdef __cplusplus
}
#endif

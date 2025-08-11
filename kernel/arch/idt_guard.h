#pragma once
#include <stdint.h>

// Install a fresh IDT with safe handlers; idempotent.
static void idt_guard_init_once(void);

// Optionally replace the muxed handlers after your real trap code is ready.
void idt_guard_install_real_handlers(void (*ud_noerr)(void*),
                                     void (*gp_err)(void*, uint64_t));

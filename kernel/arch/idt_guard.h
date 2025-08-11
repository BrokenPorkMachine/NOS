// kernel/arch/idt_guard.h
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void idt_guard_init_once(void); // no-op if you don’t link the .c
#ifdef __cplusplus
}
#endif

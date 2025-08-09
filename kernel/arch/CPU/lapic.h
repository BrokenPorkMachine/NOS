#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize Local APIC MMIO at 'base' and enable it via SVR. */
void lapic_init(uintptr_t base);

/* Return this CPU's Local APIC ID (xAPIC mode). */
uint32_t lapic_get_id(void);

/* Signal End-Of-Interrupt to the LAPIC. */
void lapic_eoi(void);

/* Send a fixed IPI with 'vector' to a specific APIC ID (physical dest). */
void lapic_send_ipi(uint8_t apic_id, uint8_t vector);

/* Send INIT IPI to 'apic_id'. */
void lapic_send_init(uint8_t apic_id);

/* Send Startup IPI (SIPI) with 'vector' to 'apic_id'. */
void lapic_send_startup(uint8_t apic_id, uint8_t vector);

/* ---------- Optional timer helpers (xAPIC MMIO) ---------- */
/* Configure LAPIC timer in periodic mode. 'vector' is the IRQ vector,
   'initial_count' the starting count, 'tdcr_div_code' one of:
   0xB(÷1), 0x0(÷2), 0x1(÷4), 0x2(÷8), 0x3(÷16), 0x8(÷32), 0x9(÷64), 0xA(÷128). */
void lapic_timer_setup_periodic(uint8_t vector, uint32_t initial_count, uint8_t tdcr_div_code);

/* Configure LAPIC timer in one-shot mode. */
void lapic_timer_setup_oneshot(uint8_t vector, uint32_t initial_count, uint8_t tdcr_div_code);

/* Read current timer count. */
uint32_t lapic_timer_current(void);

#ifdef __cplusplus
}
#endif

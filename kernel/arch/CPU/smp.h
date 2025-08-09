#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize SMP subsystem and bootstrap all Application Processors (APs).
   Requires valid bootinfo_t from bootloader containing APIC IDs. */
void smp_bootstrap(const bootinfo_t *bi);

/* Return the Local APIC ID of the currently running CPU. */
uint32_t smp_cpu_id(void);

/* Return the zero-based logical index of the current CPU in the system.
   Falls back to 0 if the APIC ID is unmapped. */
uint32_t smp_cpu_index(void);

/* Return the total number of CPUs detected and initialized by SMP. */
uint32_t smp_cpu_count(void);

/* -------- Optional future SMP helpers -------- */

/* Return the logical index for a given APIC ID, or 0xFFFFFFFF if invalid. */
uint32_t smp_apic_to_index(uint32_t apic_id);

/* Return the APIC ID for a given logical CPU index, or 0xFFFFFFFF if invalid. */
uint32_t smp_index_to_apic(uint32_t cpu_index);

/* Mark an AP as online — call from AP entry code once it's ready. */
void smp_mark_online(uint32_t apic_id);

/* Return nonzero if all APs are online (for post-bring-up sync). */
int smp_all_online(void);

#ifdef __cplusplus
}
#endif

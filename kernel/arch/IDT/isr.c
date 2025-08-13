#include <string.h>
#include "arch/IDT/isr.h"

/* Example timer handler that hands off to scheduler safely:
   make an aligned local copy to avoid taking address of a packed member. */
void isr_timer_handler(struct isr_context *ctx) {
    /* local aligned copy */
    struct isr_context local;
    memcpy(&local, ctx, sizeof local);

    /* scheduler can inspect the frame if it wants */
    uint64_t *new_rsp = schedule_from_isr((const void*)&local);

    /* If your scheme returns a new RSP to install, write it back: */
    if (new_rsp) {
        ctx->rsp = (uint64_t)new_rsp;
    }
}

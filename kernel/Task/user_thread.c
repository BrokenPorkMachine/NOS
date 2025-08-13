#include <string.h>
#include "user_thread.h"
#include "../arch/GDT/segments.h"

void init_user_thread(cpu_context *ctx, uint64_t entry, uint64_t user_stack) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->rip    = entry;
    ctx->cs     = GDT_SEL_USER_CODE_R3;
    ctx->rflags = 0x202;            /* IF=1 */
    ctx->rsp    = user_stack;
    ctx->ss     = GDT_SEL_USER_DATA_R3;
}

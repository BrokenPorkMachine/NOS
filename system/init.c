// ============================================================================
// system/init.c  -- integrate dyld2 in your existing init agent
// ============================================================================
#include "../rt/agent_abi.h"
#include "../libc/libc.h"
#include "../loader/dyld2.h"
#include "regx_key.h"

extern int kprintf(const char *fmt, ...);

void init_main(const AgentAPI *api, uint32_t self_tid)
{
    (void)self_tid;
    if(!api) return;
    if (regx_verify_launch_key(REGX_LAUNCH_KEY) != 0) {
        kprintf("[init] invalid launch key\n");
        return;
    }
    if(api->puts) api->puts("[init] starting with dyld2\n");
    kprintf("[init] starting with dyld2\n");
    dyld2_init(api);

    const char* login_path = "/agents/login.mo2";
    if(api->puts) api->puts("[init] launching login\n");
    kprintf("[init] launching login\n");
    int tid = -1;
    if (api->regx_load)
        tid = api->regx_load(login_path, NULL, NULL);
    if(api->printf) api->printf("[init] login launched tid=%d\n", tid);
    kprintf("[init] login launched tid=%d\n", tid);

    if(api->puts) api->puts("[init] idle loop\n");
    for(;;){ if(api->yield) api->yield(); for(volatile int i=0;i<200000;i++) __asm__ __volatile__("pause"); }
}

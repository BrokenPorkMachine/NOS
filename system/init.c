// ============================================================================
// system/init.c  -- integrate dyld2 in your existing init agent
// ============================================================================
#include "../rt/agent_abi.h"
#include "../libc/libc.h"
#include "../loader/dyld2.h"

void init_main(const AgentAPI *api, uint32_t self_tid)
{
    (void)self_tid; if(!api) return; if(api->puts) api->puts("[init] starting with dyld2\n");
    dyld2_init(api);

    // Option A: load dyld2-managed executables explicitly
    const char* login_path = "/agents/login.mo2"; // dynamic or static
    const char* argvv[2] = { "login", 0 };
    int rc = dyld2_run_exec(login_path, 1, argvv);
    if(api->printf) api->printf("[init] login exited rc=%d\n", rc);

    if(api->puts) api->puts("[init] idle loop\n");
    for(;;){ if(api->yield) api->yield(); for(volatile int i=0;i<200000;i++) __asm__ __volatile__("pause"); }
}

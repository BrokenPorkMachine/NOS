#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"
#include "dyld2.h"
#include "regx_key.h"

extern int kprintf(const char *fmt, ...);

/* Minimal manifest so the loader can discover the entry point when the
 * agent is packaged as a Mach-O2 binary. */
__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": 4,\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"_start\"\n"
"}\n";

/* Entry point for the init agent.  It now acts as a userspace agent loader
 * using the dyld2 runtime to start other agents in a sandboxed manner. */
void init_main(const AgentAPI *api, uint32_t self_tid)
{
    (void)self_tid;
    if (!api)
        return;

    if (regx_verify_launch_key(REGX_LAUNCH_KEY) != 0) {
        kprintf("[init] invalid launch key\n");
        return;
    }

    if (api->puts)
        api->puts("[init] starting with dyld2\n");
    kprintf("[init] starting with dyld2\n");

    dyld2_init(api);

    const char *login_path = "agents/login.mo2";
    if (api->puts)
        api->puts("[init] launching login\n");
    kprintf("[init] launching login\n");
    int tid = -1;
    if (api->regx_load)
        tid = api->regx_load(login_path, NULL, NULL);
    if (api->printf)
        api->printf("[init] login launched tid=%d\n", tid);
    kprintf("[init] login launched tid=%d\n", tid);

    if (api->puts)
        api->puts("[init] idle loop\n");

    for (;;) {
        if (api->yield)
            api->yield();
        for (volatile int i = 0; i < 200000; i++)
            __asm__ __volatile__("pause");
    }
}

#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"
#include "dyld2.h"
#include <stdint.h>

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
void agent_main(void) {
    const AgentAPI *api = NOS;
    uint32_t self_tid = NOS_TID;
    (void)self_tid;
    if (!api) return;

    if (api->puts) api->puts("[init] starting with dyld2\n");
    dyld2_init(api);

    const char *login_path = "agents/login.mo2";
    const char *argvv[2] = { "login", 0 };
    int rc = dyld2_run_exec(login_path, 1, argvv);
    if (api->printf) api->printf("[init] login exited rc=%d\n", rc);

    if (api->puts) api->puts("[init] idle loop\n");
    for (;;) {
        if (api->yield) api->yield();
    }
}


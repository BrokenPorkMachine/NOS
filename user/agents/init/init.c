#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"
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

/* Entry point for the init agent.  It simply launches the login agent and
 * then yields forever. */
void init_main(const AgentAPI *api, uint32_t self_tid) {
    (void)self_tid;
    if (!api) return;

    if (api->puts) api->puts("[init] starting\n");

    if (api->regx_load) {
        int rc = api->regx_load("/agents/login.mo2", NULL, NULL);
        if (api->printf)
            api->printf("[init] launch login rc=%d\n", rc);
    }

    if (api->puts) api->puts("[init] bootstrap complete\n");

    for (;;) {
        if (api->yield) api->yield();
    }
}


#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"
#include "dyld2.h"
#include <stdint.h>

static inline void dbg_char(char c){ __asm__ __volatile__("outb %0,$0xe9"::"a"(c)); }
static inline void dbg_str(const char*s){ while(*s) dbg_char(*s++); }

/* Minimal manifest so the loader can discover the entry point when the
 * agent is packaged as a Mach-O2 binary. */
__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": 4,\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"init_main\"\n"
"}\n";

/* Entry point for the init agent.  It now acts as a userspace agent loader
 * using the dyld2 runtime to start other agents in a sandboxed manner. */
void init_main(const AgentAPI *api, uint32_t self_tid) {
    dbg_str("[init] entered\n");
    (void)self_tid;
    if (!api) { dbg_str("[init] api=NULL\n"); for(;;)__asm__ __volatile__("hlt"); }

    if (api->puts) api->puts("[init] starting with dyld2\n");
    else dbg_str("[init] puts=NULL\n");
    dyld2_init(api);

    const char *login_path = "agents/login.mo2";
    const char *argvv[2] = { "login", 0 };
    int rc = dyld2_run_exec(login_path, 1, argvv);
    if (api->printf)
        api->printf("[init] login exited rc=%d\n", rc);
    else
        dbg_str("[init] printf=NULL\n");

    if (api->puts) api->puts("[init] idle loop\n");
    else dbg_str("[init] idle\n");
    for (;;) {
        if (api->yield) api->yield();
        __asm__ __volatile__("pause");
    }
}


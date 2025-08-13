#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"
#include "dyld2.h"
#include <stdint.h>

static inline void dbg_char(char c){ __asm__ __volatile__("outb %0,$0xe9"::"a"(c)); }

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
static inline void dbg_ch(char c){ __asm__ __volatile__("outb %0,$0xe9"::"a"(c)); }
static inline void dbg_str(const char*s){ while(*s) dbg_ch(*s++); }
static inline void dbg_hex(uint64_t v){ for(int i=60;i>=0;i-=4){ int n=(v>>i)&0xF; dbg_ch(n<10?'0'+n:'A'+n-10); } }

void init_main(const AgentAPI *api, uint32_t self_tid) {
    dbg_str("[init] entered api="); dbg_hex((uint64_t)api);
    dbg_str(" tid="); dbg_hex(self_tid); dbg_ch('\n');

    if (api && api->puts) api->puts("[init] starting\n");
    else dbg_str("[init] puts=NULL\n");

    int rc = -1;
    if (api && api->regx_load) rc = api->regx_load("/agents/login.mo2", NULL, NULL);
    else dbg_str("[init] regx_load=NULL\n");

    if (api && api->printf) api->printf("[init] launch login rc=%d\n", rc);
    else dbg_str("[init] printf=NULL\n");

    if (api && api->puts) api->puts("[init] bootstrap complete\n");
    else dbg_str("[init] done\n");

    for(;;){ if (api && api->yield) api->yield(); __asm__ __volatile__("pause"); }
}

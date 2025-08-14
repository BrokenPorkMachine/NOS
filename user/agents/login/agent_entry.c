#include "../../libc/libc.h"
#include "../../rt/agent_abi.h"
#include "login.h"

// Real entry implemented in login.c
void login_server(void *fs_q, uint32_t self_id);
void thread_yield(void);

// Manifest describing this agent
__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char mo2_manifest[] =
"{\n"
"  \"name\": \"login\",\n"
"  \"type\": 4,\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"_start\"\n"
"}\n";

// Runtime entry expected by rt0_user.S
void _agent_main(const AgentAPI *api, uint32_t self_id) {
    (void)api; // API currently unused by the login server
    login_server(NULL, self_id);
    for (;;) thread_yield();
}


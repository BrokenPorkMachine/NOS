#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"
#include "login.h"

// Real entry implemented in login.c
void login_server(ipc_queue_t *fs_q, uint32_t self_id);
void thread_yield(void);

// Manifest describing this agent
__attribute__((section("__O2INFO,__manifest")))
static const char mo2_manifest[] =
"{\n"
"  \"name\": \"login\",\n"
"  \"type\": \"service\",\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"agent_main\"\n"
"}\n";

// Single entry point expected by the loader
void agent_main(void) {
    login_server(NULL, 0);
    for (;;) thread_yield();
}


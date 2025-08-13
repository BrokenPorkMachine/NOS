#include "../rt/agent_abi.h"
extern int main(int, const char**);

void init_main(const AgentAPI *api, unsigned self_tid) {
    (void)api;
    (void)self_tid;
    main(0, NULL);
    for (;;) __asm__ __volatile__("hlt");
}

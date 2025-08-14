#include "../../rt/agent_abi.h"

// Forward declaration of the real init entry point
void init_main(const AgentAPI *api, uint32_t self_tid);

// Runtime entry expected by rt0_user.S
void _agent_main(const AgentAPI *api, uint32_t self_tid) {
    init_main(api, self_tid);
}

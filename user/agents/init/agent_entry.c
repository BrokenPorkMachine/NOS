// user/agents/init/agent_entry.c
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/IPC/ipc.h"

// init_main is implemented in init.c
void init_main(ipc_queue_t *q, unsigned self_id);

void agent_main(void) { init_main(NOS, NOS_TID); for(;;) NOS->yield(); }

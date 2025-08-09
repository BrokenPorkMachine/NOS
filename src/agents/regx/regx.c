// user/agents/regx/regx.c
#include "../../libc/libc.h"
#include <stdint.h>
#include "../../libc/libc.h"
#include "../../../include/ipc_types.h"   // adjust include path to your IPC types if needed

// Console (kernel printf is linked; calling through is fine if exported)
extern int kprintf(const char *fmt, ...);

// Loader API (provided by kernel)
extern int agent_loader_run_from_path(const char *path, int prio);

// Optional: register self with kernel (depends on your agent.h, regx APIs)
extern void regx_ready_signal(void);    // if you have a signal to mark regx up; else omit

void regx_main(void) {
    kprintf("[regx] up: launching init as standalone agent\n");

    // Start init; it will bring up the rest of the agents from /agents
    if (agent_loader_run_from_path("/agents/init.bin", 200) < 0) {
        kprintf("[regx] failed to launch init\n");
    }

    // (Optional) bring up anything else you want before init, or wait for requests
    // Example idle loop:
    for (;;) {
        __asm__ volatile("pause");
    }
}

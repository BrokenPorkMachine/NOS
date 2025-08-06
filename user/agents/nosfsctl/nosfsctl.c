#include "../nosfs/nosfs_server.h"
#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../../include/agent.h"
#include <string.h>
#include <stdio.h>

/* Simple demonstration of interacting with the NOSFS agent via IPC.  Real
 * implementations would discover the filesystem agent ID via syscalls or the
 * agent registry. */
int main(void) {
    ipc_queue_t q;
    ipc_message_t msg = {0}, reply = {0};

    ipc_queue_init(&q); // stubbed helper from libc

    /* Discover the filesystem agent dynamically and request a directory
     * listing. */
    const n2_agent_t *fs = n2_agent_find_capability("filesystem");
    if (!fs)
        return 1; /* agent not found */
    msg.type = NOSFS_MSG_LIST;
    ipc_send(&q, fs->id, &msg);      // send using agent registry ID
    ipc_receive_blocking(&q, fs->id, &reply);

    if (reply.arg1 > 0) {
        size_t count = reply.arg1;
        for (size_t i = 0; i < count; ++i) {
            printf("%s\n", &reply.data[i * NOSFS_NAME_LEN]);
        }
    }
    return 0;
}

#include <assert.h>
#include <string.h>
#include "nosm_ipc.h"

int main(void) {
    ipc_message_t m = {0};
    uint8_t blob[3] = {1,2,3};
    nosm_ipc_build_verify(&m, 42, blob, sizeof(blob));
    assert(m.type == NOSM_IPC_VERIFY_REQ);
    assert(m.len == 8 + sizeof(blob));
    assert(((uint32_t*)m.data)[0] == 42);
    assert(memcmp(m.data+8, blob, sizeof(blob)) == 0);
    return 0;
}


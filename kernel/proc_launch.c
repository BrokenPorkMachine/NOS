#include "uaccess.h"
#include "abi_sanitize.h"
#include "abi.h"

__attribute__((weak))
int create_user_process(uintptr_t entry, uintptr_t user_stack_top,
                        const char* const *argv, const char* const *envp) {
    (void)entry; (void)user_stack_top; (void)argv; (void)envp; return -1;
}
extern void kprintf(const char *fmt, ...);

int launch_init_from_registry(const ipc_msg_t *msg) {
    uintptr_t entry = 0;
    int rc = abi_get_user_ptr_u64(msg->buffer, &entry);
    if (rc) {
        kprintf("[regx] bad entry ptr 0x%llx\n", (unsigned long long)msg->buffer);
        return rc;
    }
    CANONICAL_GUARD(entry);
    return create_user_process(entry, 0, NULL, NULL);
}

int copy_args_to_user(uintptr_t user_sp, const char *src, size_t len) {
    if (!range_add_ok(user_sp, len) || !is_user_addr(user_sp) ||
        !range_is_mapped_user(user_sp, len)) {
        return -14;
    }
    CANONICAL_GUARD(user_sp);
    return copy_to_user((void*)user_sp, src, len);
}

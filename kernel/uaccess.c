#include "uaccess.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *dst, int c, size_t n);
extern void kprintf(const char *fmt, ...);

int copy_from_user(void *dst, const void *user_src, size_t n) {
    uintptr_t a = (uintptr_t)user_src;
    if (!range_add_ok(a, n) || !is_user_addr(a) || !range_is_mapped_user(a, n)) {
        return -14; /* -EFAULT */
    }
    memcpy(dst, (const void *)a, n);
    return 0;
}

int copy_to_user(void *user_dst, const void *src, size_t n) {
    uintptr_t a = (uintptr_t)user_dst;
    if (!range_add_ok(a, n) || !is_user_addr(a) || !range_is_mapped_user(a, n)) {
        return -14; /* -EFAULT */
    }
    memcpy((void *)a, src, n);
    return 0;
}

void __kernel_panic_noncanonical(uint64_t addr, const char* file, int line) {
    kprintf("[uaccess] NON-CANONICAL PTR: 0x%016llx at %s:%d\n",
            (unsigned long long)addr, file, line);
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

#ifdef UACCESS_SELFTEST
void uaccess_selftest(void) {
    char buf[4];
    int rc;
    rc = copy_from_user(buf, (const void*)0x1000, sizeof(buf));
    kprintf("[selftest] low rc=%d\n", rc);
    rc = copy_from_user(buf, (const void*)(USER_TOP + 1ULL), sizeof(buf));
    kprintf("[selftest] past_top rc=%d\n", rc);
    rc = copy_from_user(buf, (const void*)0xFFFF000000000000ULL, sizeof(buf));
    kprintf("[selftest] noncanon rc=%d\n", rc);
    bool ok = range_add_ok(UINT64_MAX - 1ULL, 4U);
    kprintf("[selftest] wrap_ok=%d\n", ok ? 1 : 0);
}
#endif

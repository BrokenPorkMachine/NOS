#include <assert.h>
#include <string.h>
#include "../../user/servers/login/login.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"
#include <sys/mman.h>

static const char *input = "admin\nadmin\n";
static size_t pos = 0;

int keyboard_getchar(void) {
    if (pos >= strlen(input)) return -1;
    return (unsigned char)input[pos++];
}

void serial_write(char c) { (void)c; }
void serial_puts(const char *s) { (void)s; }
void thread_yield(void) { }

int main(void) {
    ipc_queue_t q; (void)q;
    mmap((void*)0xB8000, 0x1000, PROT_READ|PROT_WRITE,
         MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    login_done = 0;
    login_server(&q, 0);
    assert(login_done == 1);
    return 0;
}

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

// Stubs for external dependencies of regx.c
int kprintf(const char *fmt, ...) { (void)fmt; return 0; }
void thread_yield(void) {}
int nosfs_is_ready(void) { return 1; }
void serial_puts(const char *s) { (void)s; }
void serial_putsn(const char *s, size_t n) { (void)s; (void)n; }
void agent_loader_set_gate(int (*gate)(const char*, const char*, const char*, const char*, const char*)) { (void)gate; }

static const char *last_path;
int agent_loader_run_from_path(const char *path, int prio) {
    (void)prio;
    last_path = path;
    return 1; // fake thread id
}

// Include the implementation under test
#include "../../src/agents/regx/regx.c"

int main(void) {
    // Absolute path should be normalized
    regx_load("/agents/login.mo2", NULL, NULL);
    assert(strcmp(last_path, "agents/login.mo2") == 0);

    // Relative path should remain unchanged
    regx_load("agents/login.mo2", NULL, NULL);
    assert(strcmp(last_path, "agents/login.mo2") == 0);
    return 0;
}

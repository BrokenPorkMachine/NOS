#include <stdint.h>
#include <stddef.h>

void context_switch(uint64_t *prev, uint64_t next) { (void)prev; (void)next; }
void regx_main(void) {}
void nosm_entry(void) {}
void nosfs_server(void *q, uint32_t id) { (void)q; (void)id; }
void login_server(void *q, uint32_t id) { (void)q; (void)id; }
int fs_read_all(const char *path, void **out, size_t *out_sz) { (void)path; (void)out; (void)out_sz; return -1; }
void agent_loader_set_read(int (*reader)(const char*, void**, size_t*), void (*freer)(void*)) { (void)reader; (void)freer; }
void ipc_init(void *q) { (void)q; }
void ipc_grant(void *q, uint32_t id, uint32_t caps) { (void)q; (void)id; (void)caps; }
int agent_loader_run_from_path(const char *path, int prio) { (void)path; (void)prio; return -1; }
void serial_puts(const char *s) { (void)s; }
void arm_init_watchdog(void) {}
int api_puts(const char *s) { (void)s; return 0; }
int api_fs_read_all(const char *path, void *buf, size_t len, size_t *outlen) { (void)path; (void)buf; (void)len; if (outlen) *outlen = 0; return -1; }
int api_regx_load(const char *name, const char *arg, uint32_t *out) { (void)name; (void)arg; if (out) *out = 0; return -1; }
void api_yield(void) {}
int regx_verify_launch_key(const char *key) { (void)key; return 0; }

uint64_t *paging_kernel_pml4(void) { return NULL; }
void paging_switch(uint64_t *new_pml4) { (void)new_pml4; }
uint64_t *paging_new_context(void) { return NULL; }

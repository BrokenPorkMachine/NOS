#include <stdint.h>
#include <stddef.h>

uint32_t smp_cpu_index(void) { return 0; }
void context_switch(uint64_t *prev, uint64_t next) { (void)prev; (void)next; }
void regx_main(void) {}
void nosm_entry(void) {}
void nosfs_server(void *q, uint32_t id) { (void)q; (void)id; }
void login_server(void *q, uint32_t id) { (void)q; (void)id; }
int fs_read_all(const char *path, void **out, size_t *out_sz) { (void)path; (void)out; (void)out_sz; return -1; }
void agent_loader_set_read(int (*reader)(const char*, void**, size_t*), void (*freer)(void*)) { (void)reader; (void)freer; }
void ipc_init(void *q) { (void)q; }
void ipc_grant(void *q, uint32_t id, uint32_t caps) { (void)q; (void)id; (void)caps; }

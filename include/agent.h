#ifndef NOS_AGENT_H
#define NOS_AGENT_H

#include <stddef.h>
#include <stdint.h>

#define N2_MAX_AGENTS 32

typedef struct {
    uint32_t id;
    char name[32];
    char version[16];
    void *entry;
    const void *manifest;
    char capabilities[64];  // safe capabilities string
} n2_agent_t;

int n2_agent_register(const n2_agent_t *agent);
const n2_agent_t *n2_agent_get(const char *name);
void n2_agent_registry_reset(void);
int n2_agent_unregister(const char *name);
size_t n2_agent_list(n2_agent_t *out, size_t max);
const n2_agent_t *n2_agent_find_capability(const char *cap);

#endif /* NOS_AGENT_H */

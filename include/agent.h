#ifndef NOS_AGENT_H
#define NOS_AGENT_H

#include <stddef.h>

#define N2_MAX_AGENTS 32

typedef struct {
    char name[32];
    char version[16];
    void *entry;
    const void *manifest;
} n2_agent_t;

int n2_agent_register(const n2_agent_t *agent);
const n2_agent_t *n2_agent_get(const char *name);
void n2_agent_registry_reset(void);

#endif /* NOS_AGENT_H */

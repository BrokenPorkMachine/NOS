#include "agent.h"
#include <string.h>

static n2_agent_t registry[N2_MAX_AGENTS];
static size_t registry_count;

void n2_agent_registry_reset(void) {
    registry_count = 0;
}

int n2_agent_register(const n2_agent_t *agent) {
    if (!agent || registry_count >= N2_MAX_AGENTS)
        return -1;
    registry[registry_count] = *agent;
    registry_count++;
    return 0;
}

const n2_agent_t *n2_agent_get(const char *name) {
    for (size_t i = 0; i < registry_count; ++i) {
        if (strncmp(registry[i].name, name, sizeof(registry[i].name)) == 0)
            return &registry[i];
    }
    return NULL;
}

int n2_agent_unregister(const char *name) {
    if (!name)
        return -1;
    for (size_t i = 0; i < registry_count; ++i) {
        if (strncmp(registry[i].name, name, sizeof(registry[i].name)) == 0) {
            memmove(&registry[i], &registry[i + 1],
                    (registry_count - i - 1) * sizeof(n2_agent_t));
            registry_count--;
            return 0;
        }
    }
    return -1;
}

size_t n2_agent_list(n2_agent_t *out, size_t max) {
    if (!out || max == 0)
        return 0;
    size_t n = registry_count < max ? registry_count : max;
    memcpy(out, registry, n * sizeof(n2_agent_t));
    return n;
}

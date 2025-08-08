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

    /* ensure name is null-terminated within the buffer */
    if (!memchr(agent->name, '\0', sizeof(agent->name)))
        return -1;

    /* avoid duplicate registrations */
    if (n2_agent_get(agent->name))
        return -1;

    registry[registry_count] = *agent;
    registry[registry_count].id = (uint32_t)registry_count;
    registry_count++;
    return 0;
}

const n2_agent_t *n2_agent_get(const char *name) {
    if (!name)
        return NULL;

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

const n2_agent_t *n2_agent_find_capability(const char *cap) {
    if (!cap)
        return NULL;
    for (size_t i = 0; i < registry_count; ++i) {
        const char *man = (const char *)registry[i].manifest;
        if (!man)
            continue;
        /* Only treat manifest as a string if a NUL appears within 256 bytes. */
        if (!memchr(man, '\0', 256))
            continue;
        if (strstr(man, cap))
            return &registry[i];
    }
    return NULL;
}

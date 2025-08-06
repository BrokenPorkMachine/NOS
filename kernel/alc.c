#include <stdlib.h>
#include <string.h>
#include "alc.h"
#include "agent.h"

/* Load a cache image already present in memory */
int alc_load_cache(const void *image, size_t size, alc_cache_t *cache) {
    if (!image || !cache || size < sizeof(alc_header_t))
        return -1;
    const alc_header_t *hdr = (const alc_header_t *)image;
    if (hdr->magic != ALC_MAGIC)
        return -1;
    cache->hdr = hdr;
    cache->entries = (const alc_entry_t *)((const uint8_t *)image + hdr->index_offset);
    cache->usage = calloc(hdr->entry_count, sizeof(alc_usage_t));
    return cache->usage ? 0 : -1;
}

/* Register all entries from a cache with the agent registry */
int alc_register_cache(const alc_cache_t *cache) {
    if (!cache || !cache->hdr)
        return -1;
    for (uint32_t i = 0; i < cache->hdr->entry_count; ++i) {
        n2_agent_t agent = {0};
        const alc_entry_t *e = &cache->entries[i];
        strncpy(agent.name, e->name, sizeof(agent.name) - 1);
        strncpy(agent.version, e->version, sizeof(agent.version) - 1);
        n2_agent_register(&agent);
    }
    return 0;
}

const alc_entry_t *alc_find(const alc_cache_t *cache,
                            const char *name,
                            const char *version,
                            const char *abi) {
    if (!cache || !name)
        return NULL;
    for (uint32_t i = 0; i < cache->hdr->entry_count; ++i) {
        const alc_entry_t *e = &cache->entries[i];
        if (strcmp(e->name, name) == 0 &&
            (!version || strcmp(e->version, version) == 0) &&
            (!abi || strcmp(e->abi, abi) == 0)) {
            return e;
        }
    }
    return NULL;
}

void *alc_map(const alc_entry_t *entry) {
    /* Real implementation would mmap the blob and apply relocations. */
    return (void *)(uintptr_t)entry->blob_offset;
}

int alc_validate(const alc_entry_t *entry) {
    /* Placeholder hash/signature check. */
    (void)entry;
    return 0;
}

/* Hot reload by swapping an old entry pointer for a new one */
int alc_hot_reload(alc_cache_t *cache,
                   const alc_entry_t *old_entry,
                   const alc_entry_t *new_entry) {
    if (!cache || !old_entry || !new_entry)
        return -1;
    if (strcmp(old_entry->abi, new_entry->abi) != 0)
        return -1;
    /* migrate usage counters */
    size_t idx = old_entry - cache->entries;
    cache->usage[idx].hits = 0;
    cache->usage[idx].last_used = 0;
    /* in practice the registry would repoint handles here */
    return 0;
}

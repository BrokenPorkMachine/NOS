#include "agent_loader.h"
#include "regx.h"
#include "json.h"
#include "nosm.h"
#include <stdio.h>
#include <string.h>

/* Stub implementations. Real kernel would map segments and create threads. */
int map_segment(const void *data, size_t size, uint64_t vaddr) {
    (void)data; (void)size; (void)vaddr;
    return 0;
}

int spawn_thread(void (*entry)(void)) {
    if (entry) entry();
    return 0;
}

/* Entry registry --------------------------------------------------------- */
typedef struct { const char *name; agent_entry_t fn; } entry_map_t;
#define MAX_ENTRY_FUNCS 16
static entry_map_t entry_map[MAX_ENTRY_FUNCS];
static size_t entry_map_count = 0;

int agent_loader_register_entry(const char *name, agent_entry_t fn) {
    if (entry_map_count >= MAX_ENTRY_FUNCS) return -1;
    entry_map[entry_map_count].name = name;
    entry_map[entry_map_count].fn = fn;
    entry_map_count++;
    return 0;
}

agent_entry_t agent_loader_find_entry(const char *name) {
    for (size_t i = 0; i < entry_map_count; ++i) {
        if (strcmp(entry_map[i].name, name) == 0)
            return entry_map[i].fn;
    }
    return NULL;
}

/* Format detection ------------------------------------------------------- */
agent_format_t detect_agent_format(const void *image, size_t size) {
    if (!image || size < 4) return AGENT_FORMAT_UNKNOWN;
    const unsigned char *p = image;
    if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
        return AGENT_FORMAT_ELF;
    uint32_t magic = *(const uint32_t *)p;
    if (magic == 0xfeedface || magic == 0xcefaedfe ||
        magic == 0xfeedfacf || magic == 0xcffaedfe)
        return AGENT_FORMAT_MACHO;
    if (magic == 0x4e4f534d) /* 'NOSM' */
        return AGENT_FORMAT_NOSM;
    if (p[0] == '{')
        return AGENT_FORMAT_MACHO2; /* JSON header */
    return AGENT_FORMAT_FLAT;
}

/* Manifest helpers ------------------------------------------------------- */
int extract_manifest_macho2(const void *image, size_t size,
                            char *out_json, size_t out_size) {
    if (!image || !out_json || out_size == 0) return -1;
    if (size >= out_size) size = out_size - 1;
    memcpy(out_json, image, size);
    out_json[size] = '\0';
    return 0;
}

int extract_manifest_elf(const void *image, size_t size,
                         char *out_json, size_t out_size) {
    if (!image || !out_json || out_size == 0) return -1;
    const unsigned char *p = image;
    for (size_t i = 0; i < size; ++i) {
        if (p[i] == '{') {
            size_t rem = size - i;
            if (rem >= out_size) rem = out_size - 1;
            memcpy(out_json, p + i, rem);
            out_json[rem] = '\0';
            return 0;
        }
    }
    return -1;
}

/* Internal helper to parse manifest and start agent. */
static int load_from_manifest(const char *json) {
    regx_manifest_t m;
    memset(&m, 0, sizeof(m));
    if (json_get_string(json, "name", m.name, sizeof(m.name))) return -1;
    json_get_string(json, "type", m.type, sizeof(m.type));
    json_get_string(json, "version", m.version, sizeof(m.version));
    json_get_string(json, "abi", m.abi, sizeof(m.abi));
    json_get_string(json, "capabilities", m.capabilities, sizeof(m.capabilities));
    json_get_string(json, "entry", m.entry, sizeof(m.entry));

    regx_register(&m, 0);
    agent_entry_t fn = agent_loader_find_entry(m.entry);
    if (fn)
        spawn_thread(fn);
    return 0;
}

/* Loader implementations ------------------------------------------------- */
int load_agent_macho2(const void *image, size_t size) {
    char manifest[512];
    if (extract_manifest_macho2(image, size, manifest, sizeof(manifest)))
        return -1;
    return load_from_manifest(manifest);
}

int load_agent_elf(const void *image, size_t size) {
    char manifest[512];
    if (extract_manifest_elf(image, size, manifest, sizeof(manifest)))
        return -1;
    return load_from_manifest(manifest);
}

int load_agent_macho(const void *image, size_t size) {
    (void)image; (void)size;
    /* Real implementation would parse Mach-O headers and symbols. */
    return -1;
}

int load_agent_flat(const void *image, size_t size) {
    (void)image; (void)size;
    /* Flat binaries would start executing at the buffer start. */
    return -1;
}

int load_agent_nosm(const void *image, size_t size) {
    return nosm_load(image, size) ? 0 : -1;
}

int load_agent_auto(const void *image, size_t size) {
    switch (detect_agent_format(image, size)) {
    case AGENT_FORMAT_ELF:
        return load_agent_elf(image, size);
    case AGENT_FORMAT_MACHO:
        return load_agent_macho(image, size);
    case AGENT_FORMAT_MACHO2:
        return load_agent_macho2(image, size);
    case AGENT_FORMAT_FLAT:
        return load_agent_flat(image, size);
    case AGENT_FORMAT_NOSM:
        return load_agent_nosm(image, size);
    default:
        return -1;
    }
}

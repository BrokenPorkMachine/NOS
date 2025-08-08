#include "agent_loader.h"
#include "agent.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Provide a simple implementation of memmem for environments where it is
// unavailable. This performs a byte-wise search of `needle` within `haystack`.
static const void *memmem_local(const void *haystack, size_t haystacklen,
                                const void *needle, size_t needlelen) {
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    if (needlelen == 0)
        return haystack;
    for (size_t i = 0; i + needlelen <= haystacklen; ++i)
        if (memcmp(h + i, n, needlelen) == 0)
            return h + i;
    return NULL;
}

// ---- Minimal JSON parser ----------------------------------------------------
// extracts a string associated with a key in a flat JSON object
static int json_extract_string(const char *json, const char *key,
                               char *out, size_t out_sz) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    size_t i = 0;
    while (*p && *p != '"' && i < out_sz-1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return 0;
}
// extracts an integer associated with a key
static int json_extract_int(const char *json, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    return (int)strtol(p, NULL, 10);
}

// ---- Internal entry registry ------------------------------------------------
#define MAX_ENTRIES 32
static struct {
    const char *name;
    agent_entry_t fn;
} entry_registry[MAX_ENTRIES];
static size_t entry_count = 0;

void agent_loader_register_entry(const char *name, agent_entry_t fn) {
    if (!name || !fn)
        return;

    for (size_t i = 0; i < entry_count; ++i)
        if (strcmp(entry_registry[i].name, name) == 0)
            return; /* already registered */

    if (entry_count < MAX_ENTRIES) {
        entry_registry[entry_count].name = name;
        entry_registry[entry_count].fn = fn;
        entry_count++;
    }
}
static agent_entry_t find_entry_fn(const char *name) {
    for (size_t i = 0; i < entry_count; ++i)
        if (strcmp(entry_registry[i].name, name) == 0)
            return entry_registry[i].fn;
    return NULL;
}

// ---- Format detection -------------------------------------------------------
agent_format_t detect_agent_format(const void *image, size_t size) {
    const unsigned char *d = (const unsigned char *)image;
    if (size >= 4 && memcmp(d, "\x7F""ELF", 4) == 0) {
        return AGENT_FORMAT_ELF;
    }
    // Mach‑O magic (little‑/big‑endian variants)
    if (size >= 4 &&
        ((d[0]==0xCF && d[1]==0xFA && d[2]==0xED && d[3]==0xFE) ||
         (d[0]==0xFE && d[1]==0xED && d[2]==0xFA && d[3]==0xCF))) {
        // detect Mach‑O2 if it contains the __O2INFO section label in plain text
        if (memmem_local(d, size, "__O2INFO", 8)) return AGENT_FORMAT_MACHO2;
        return AGENT_FORMAT_MACHO;
    }
    // NOSM: starts with “NOSM”
    if (size >= 4 && memcmp(d, "NOSM", 4) == 0)
        return AGENT_FORMAT_NOSM;
    // If manifest appears to be plain JSON, treat as Mach‑O2 container
    if (size > 0 && d[0] == '{')
        return AGENT_FORMAT_MACHO2;
    // Otherwise treat as flat
    return AGENT_FORMAT_FLAT;
}

// ---- Manifest extractors -----------------------------------------------------
int extract_manifest_macho2(const void *image, size_t size,
                            char *out_json, size_t out_sz) {
    // very simple parser: search for “__O2INFO” followed by a JSON string
    const unsigned char *d = (const unsigned char *)image;
    const char *start = memmem_local(d, size, "{", 1);
    const char *end   = memmem_local(d, size, "}", 1);
    if (!start || !end || end <= start || (size_t)(end-start+2) > out_sz)
        return -1;
    memcpy(out_json, start, end-start+2);
    out_json[end-start+1] = 0;
    return 0;
}
int extract_manifest_elf(const void *image, size_t size,
                         char *out_json, size_t out_sz) {
    // simplified: look for a '{' and copy until matching '}'
    const unsigned char *d = (const unsigned char *)image;
    const char *s = memchr(d, '{', size);
    const char *e = memchr(d, '}', size);
    if (!s || !e || e <= s || (size_t)(e-s+2) > out_sz)
        return -1;
    memcpy(out_json, s, e-s+2);
    out_json[e-s+1] = 0;
    return 0;
}

// ---- Format‑specific loaders -------------------------------------------------
static int register_from_manifest(const char *json) {
    regx_manifest_t m = {0};
    char entry[32] = {0};
    // parse manifest fields; fallback if missing
    json_extract_string(json, "name", m.name, sizeof(m.name));
    m.type = json_extract_int(json, "type");
    json_extract_string(json, "version", m.version, sizeof(m.version));
    json_extract_string(json, "abi", m.abi, sizeof(m.abi));
    json_extract_string(json, "capabilities", m.capabilities, sizeof(m.capabilities));
    json_extract_string(json, "entry", entry, sizeof(entry));
    // register with RegX, parent_id = 0 for root
    uint64_t id = regx_register(&m, 0);
    // find entry function and register agent with N2
    agent_entry_t fn = find_entry_fn(entry);
    if (fn) {
        n2_agent_t agent = {0};
        // Use snprintf to ensure strings are always null-terminated without
        // triggering truncation warnings from strncpy.
        snprintf(agent.name, sizeof(agent.name), "%s", m.name);
        snprintf(agent.version, sizeof(agent.version), "%s", m.version);
        agent.entry = fn;
        snprintf(agent.capabilities, sizeof(agent.capabilities), "%s", m.capabilities);
        agent.manifest = NULL;
        n2_agent_register(&agent);
        fn();
    } else {
        printf("[loader] entry symbol \"%s\" not found\n", entry);
    }
    return id ? 0 : -1;
}

int load_agent_macho2(const void *image, size_t size) {
    char manifest[512];
    if (extract_manifest_macho2(image, size, manifest, sizeof(manifest)) == 0)
        return register_from_manifest(manifest);
    return -1;
}

int load_agent_elf(const void *image, size_t size) {
    char manifest[512];
    if (extract_manifest_elf(image, size, manifest, sizeof(manifest)) == 0)
        return register_from_manifest(manifest);
    return -1;
}

int load_agent_macho(const void *image, size_t size) {
    (void)image;
    (void)size;
    // treat like Mach‑O2 but don’t expect manifest
    printf("[loader] Mach‑O loader not fully implemented\n");
    return 0;
}

int load_agent_flat(const void *image, size_t size) {
    (void)image;
    // flat: register with generic manifest and call first byte as entry
    regx_manifest_t m = {"flat", 0, "1.0", "flat", ""};
    regx_register(&m, 0);
    // call at base address (stub)
    printf("[loader] flat agent loaded (size=%zu)\n", size);
    return 0;
}

int load_agent_nosm(const void *image, size_t size) {
    (void)image;
    // stub loader for NOSM agent format (e.g., no manifest)
    printf("[loader] NOSM agent loaded (size=%zu)\n", size);
    return 0;
}

int load_agent_auto(const void *image, size_t size) {
    switch (detect_agent_format(image, size)) {
        case AGENT_FORMAT_MACHO2: return load_agent_macho2(image, size);
        case AGENT_FORMAT_MACHO:  return load_agent_macho(image, size);
        case AGENT_FORMAT_ELF:    return load_agent_elf(image, size);
        case AGENT_FORMAT_FLAT:   return load_agent_flat(image, size);
        case AGENT_FORMAT_NOSM:   return load_agent_nosm(image, size);
        default: return -1;
    }
}

int load_agent(const void *image, size_t size, agent_format_t fmt) {
    switch (fmt) {
        case AGENT_FORMAT_MACHO2: return load_agent_macho2(image, size);
        case AGENT_FORMAT_MACHO:  return load_agent_macho(image, size);
        case AGENT_FORMAT_ELF:    return load_agent_elf(image, size);
        case AGENT_FORMAT_FLAT:   return load_agent_flat(image, size);
        case AGENT_FORMAT_NOSM:   return load_agent_nosm(image, size);
        default:                  return load_agent_auto(image, size);
    }
}

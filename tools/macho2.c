#include "macho2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simplistic JSON helpers for manifest parsing */
static int json_extract_string(const char *json, const char *key,
                               char *out, size_t out_sz)
{
    const char *p = strstr(json, key);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '"')) p++;
    const char *e = strchr(p, '"');
    if (!e) return -1;
    size_t n = (size_t)(e - p);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return 0;
}

static uint32_t json_extract_array(const char *json, const char *key,
                                   char out[][32], uint32_t max)
{
    const char *p = strstr(json, key);
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    uint32_t count = 0;
    while (*p && *p != ']') {
        const char *q = strchr(p, '"');
        if (!q) break;
        const char *e = strchr(q + 1, '"');
        if (!e) break;
        if (count < max) {
            size_t n = (size_t)(e - q - 1);
            if (n >= 31) n = 31;
            memcpy(out[count], q + 1, n);
            out[count][n] = 0;
            count++;
        }
        p = e + 1;
    }
    return count;
}

static uint32_t json_extract_resources(const char *json,
                                       struct macho2_resource *res,
                                       uint32_t max)
{
    const char *p = strstr(json, "resources");
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    uint32_t count = 0;
    while (*p && *p != ']') {
        const char *name = strstr(p, "name");
        const char *off  = strstr(p, "offset");
        const char *sz   = strstr(p, "size");
        if (!name || !off || !sz) break;
        if (count < max) {
            json_extract_string(name, "name", res[count].name,
                                sizeof(res[count].name));
            res[count].offset = strtoull(strchr(off, ':') + 1, NULL, 0);
            res[count].size   = strtoull(strchr(sz, ':') + 1, NULL, 0);
            count++;
        }
        p = strchr(sz, '}');
        if (!p) break;
        p++;
    }
    return count;
}

int macho2_load_manifest(const char *path, struct macho2_manifest *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    struct mach_header_64 mh;
    if (fread(&mh, sizeof(mh), 1, f) != 1) { fclose(f); return -1; }
    if (mh.magic != MH_MAGIC_64) { fclose(f); return -1; }

    uint32_t ncmds = mh.ncmds;
    uint64_t manifest_off = 0, manifest_sz = 0;

    for (uint32_t i = 0; i < ncmds; ++i) {
        long pos = ftell(f);
        struct load_command lc;
        if (fread(&lc, sizeof(lc), 1, f) != 1) {
            fclose(f); return -1;
        }
        if (lc.cmd == LC_MACHO2INFO) {
            struct macho2_info_command info;
            fseek(f, pos, SEEK_SET);
            if (fread(&info, sizeof(info), 1, f) != 1) {
                fclose(f); return -1;
            }
            manifest_off = info.manifest_offset;
            manifest_sz  = info.manifest_size;
            break;
        }
        fseek(f, pos + lc.cmdsize, SEEK_SET);
    }

    if (!manifest_off || !manifest_sz) { fclose(f); return -1; }

    char *buf = malloc(manifest_sz + 1);
    if (!buf) { fclose(f); return -1; }
    fseek(f, (long)manifest_off, SEEK_SET);
    if (fread(buf, 1, manifest_sz, f) != manifest_sz) {
        free(buf); fclose(f); return -1;
    }
    buf[manifest_sz] = '\0';
    fclose(f);

    memset(out, 0, sizeof(*out));
    json_extract_string(buf, "name", out->name, sizeof(out->name));
    json_extract_string(buf, "type", out->type, sizeof(out->type));
    json_extract_string(buf, "version", out->version, sizeof(out->version));
    json_extract_string(buf, "entry", out->entry, sizeof(out->entry));
    out->privilege_count = json_extract_array(buf, "required_privileges",
                                             out->privileges, MACHO2_MAX_PRIVS);
    out->resource_count = json_extract_resources(buf, out->resources,
                                                MACHO2_MAX_RES);
    free(buf);
    return 0;
}

int macho2_extract_resource(const char *path, const struct macho2_resource *res,
                            void **buf, uint64_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, (long)res->offset, SEEK_SET)) { fclose(f); return -1; }
    void *b = malloc((size_t)res->size);
    if (!b) { fclose(f); return -1; }
    if (fread(b, 1, (size_t)res->size, f) != res->size) {
        free(b); fclose(f); return -1; }
    fclose(f);
    *buf = b;
    if (size) *size = res->size;
    return 0;
}

#ifdef MACHO2_MAIN
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <binary>\n", argv[0]);
        return 1;
    }
    struct macho2_manifest m;
    if (macho2_load_manifest(argv[1], &m)) {
        fprintf(stderr, "failed to load manifest\n");
        return 1;
    }
    printf("name: %s\nversion: %s\nentry: %s\n", m.name, m.version, m.entry);
    for (uint32_t i=0;i<m.privilege_count;i++)
        printf("priv[%u]: %s\n", i, m.privileges[i]);
    for (uint32_t i=0;i<m.resource_count;i++)
        printf("res[%u]: %s off=%llu size=%llu\n", i,
               m.resources[i].name,
               (unsigned long long)m.resources[i].offset,
               (unsigned long long)m.resources[i].size);
    return 0;
}
#endif

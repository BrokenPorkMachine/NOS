#include "pkg.h"
#include "../../libc/libc.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t version;
    uint32_t signature;
} repo_entry_t;

static repo_entry_t repo[] = {
    {"kernel", 1, 0xDEADBEEF},
    {"userland", 1, 0xCAFEBABE},
    {NULL, 0, 0}
};

typedef struct {
    char name[PKG_NAME_MAX];
    uint32_t version;
} installed_entry_t;

static installed_entry_t installed[PKG_MAX_INSTALLED];
static size_t installed_count = 0;

static uint32_t crc32_compute(const uint8_t *data, uint32_t len) {
    uint32_t crc = ~0u;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
    }
    return ~crc;
}

static repo_entry_t *find_repo(const char *name) {
    for (repo_entry_t *r = repo; r->name; ++r)
        if (!strcmp(r->name, name))
            return r;
    return NULL;
}

void pkg_init(void) {
    installed_count = 0;
}

int pkg_install(const char *name) {
    repo_entry_t *r = find_repo(name);
    if (!r)
        return -1;
    if (crc32_compute((const uint8_t*)r->name, strlen(r->name)) != r->signature)
        return -1;
    for (size_t i = 0; i < installed_count; ++i)
        if (!strcmp(installed[i].name, name))
            return 0;
    if (installed_count >= PKG_MAX_INSTALLED)
        return -1;
    strlcpy(installed[installed_count].name, r->name, PKG_NAME_MAX);
    installed[installed_count].version = r->version;
    installed_count++;
    return 0;
}

int pkg_uninstall(const char *name) {
    for (size_t i = 0; i < installed_count; ++i) {
        if (!strcmp(installed[i].name, name)) {
            for (size_t j = i + 1; j < installed_count; ++j)
                installed[j-1] = installed[j];
            installed_count--;
            return 0;
        }
    }
    return -1;
}

int pkg_list(char (*out)[PKG_NAME_MAX], uint32_t max) {
    uint32_t n = installed_count < max ? installed_count : max;
    for (uint32_t i = 0; i < n; ++i)
        strlcpy(out[i], installed[i].name, PKG_NAME_MAX);
    return n;
}

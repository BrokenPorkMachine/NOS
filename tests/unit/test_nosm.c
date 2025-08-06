#include <assert.h>
#include <string.h>
#include "nosm.h"
#include "agent.h"

static int called;
void module_entry(void) { called = 1; }

int main(void) {
    n2_agent_registry_reset();
    called = 0;

    nosm_header_t hdr = {
        .magic = NOSM_MAGIC,
        .version = 1,
        .num_segments = 1,
        .entry = 0,
        .manifest_offset = sizeof(nosm_header_t) + sizeof(nosm_segment_t),
        .manifest_size = sizeof(nosm_manifest_t)
    };

    nosm_segment_t seg = {
        .vaddr = (uint64_t)(uintptr_t)&module_entry,
        .size = 0,
        .offset = 0,
        .flags = NOSM_FLAG_X
    };

    nosm_manifest_t manifest = {"dummy","1.0"};

    unsigned char img[sizeof(hdr)+sizeof(seg)+sizeof(manifest)];
    memcpy(img, &hdr, sizeof(hdr));
    memcpy(img + sizeof(hdr), &seg, sizeof(seg));
    memcpy(img + hdr.manifest_offset, &manifest, sizeof(manifest));

    nosm_load(img, sizeof(img));

    assert(called == 1);
    const n2_agent_t *a = n2_agent_get("dummy");
    assert(a != NULL);
    assert(strcmp(a->version, "1.0") == 0);
    return 0;
}

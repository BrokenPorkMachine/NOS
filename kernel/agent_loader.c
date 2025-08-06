// agent_loader.c
#include "agent_loader.h"
#include <nosm.h>
#include "macho2.h"

// Returns 0 on success, -1 on error
int load_agent(const void *image, size_t size, enum agent_format format) {
    switch (format) {
        case AGENT_FORMAT_NOSM:
            if (!nosm_load(image, size)) return -1;
            return 0;
        case AGENT_FORMAT_MACHO2:
            if (!macho2_load(image, size)) return -1;
            return 0;
        default:
            return -1;
    }
}

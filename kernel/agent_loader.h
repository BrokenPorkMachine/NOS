// agent_loader.h
#pragma once
#include <stddef.h>

enum agent_format {
    AGENT_FORMAT_NOSM,
    AGENT_FORMAT_MACHO2,
    // Add ELF/other if you want
};

int load_agent(const void *image, size_t size, enum agent_format format);

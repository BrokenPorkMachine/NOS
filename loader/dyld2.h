// ============================================================================
// loader/dyld2.h
// ============================================================================
#ifndef DYLD2_H
#define DYLD2_H
#include "../user/rt/agent_abi.h"
#include <stddef.h>

typedef void* mo2_handle_t;
typedef void* mo2_sym_handle_t;

int          dyld2_init(const AgentAPI* api);
mo2_handle_t dyld2_dlopen(const char* path, int flags); // flags: 1=EAGER, 2=NODELETE
mo2_sym_handle_t dyld2_dlsym(mo2_handle_t h, const char* name);
int          dyld2_dlclose(mo2_handle_t h);

/* optional: start an executable image (transfer control) */
typedef int (*mo2_main_entry)(int argc, const char** argv);
int          dyld2_run_exec(const char* path, int argc, const char** argv);

#endif

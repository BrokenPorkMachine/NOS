// ============================================================================
// tools/mo2util.h  -- shared helpers for host-side tools (stdio ok)
// ============================================================================
#ifndef MO2UTIL_H
#define MO2UTIL_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../loader/mo2_format.h"

static inline void* xmalloc(size_t n){ void* p=malloc(n); if(!p){ fprintf(stderr,"oom\n"); exit(2);} return p; }
static inline char* xstrdup(const char* s){ size_t n=strlen(s)+1; char* p=(char*)xmalloc(n); memcpy(p,s,n); return p; }

#endif

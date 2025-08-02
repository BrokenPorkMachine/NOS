#pragma once
#include <stdint.h>

#define PKG_NAME_MAX 32
#define PKG_MAX_INSTALLED 16

enum {
    PKG_MSG_INSTALL = 1,
    PKG_MSG_UNINSTALL,
    PKG_MSG_LIST,
};

void pkg_init(void);
int pkg_install(const char *name);
int pkg_uninstall(const char *name);
int pkg_list(char (*out)[PKG_NAME_MAX], uint32_t max);

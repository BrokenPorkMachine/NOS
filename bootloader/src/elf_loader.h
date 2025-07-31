// src/elf_loader.h
#pragma once

#include <efi.h>
#include "bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

EFI_STATUS load_and_boot_kernel(EFI_FILE_HANDLE kernel_file, bootinfo_t *bootinfo);

#ifdef __cplusplus
}
#endif

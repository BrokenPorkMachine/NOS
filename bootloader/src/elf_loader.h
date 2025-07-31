// src/elf_loader.h
#pragma once

#include "../include/efi.h"
#include "../include/bootinfo.h"

EFI_STATUS load_and_boot_kernel(EFI_FILE_PROTOCOL *kernel_file, bootinfo_t *bootinfo,
                               EFI_BOOT_SERVICES *BS, EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut);

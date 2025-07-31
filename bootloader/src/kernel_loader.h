#pragma once
#include "../include/efi.h"
#include "../include/bootinfo.h"

EFI_STATUS load_and_boot_kernel(
    struct EFI_FILE_PROTOCOL *KernelFile,
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut,
    EFI_BOOT_SERVICES *BS,
    bootinfo_t *bootinfo);

#pragma once
#include "../include/efi.h"
#include "../include/bootinfo.h"

// Loads a kernel ELF file (already-opened), prints info, and jumps to entry.
// Returns 0 on success, error code otherwise.
EFI_STATUS load_and_boot_kernel(
    EFI_FILE_PROTOCOL *KernelFile,
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut,
    bootinfo_t *bootinfo);

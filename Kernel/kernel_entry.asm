section .text
global _start

; Symbols for kernel_main and optional stack end
extern kernel_main

; Multiboot2 magic as per the spec
%define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

_start:
    cld

    ; Set up stack
    mov     rsp, 0x200000
    xor     rbp, rbp

    ; Detect Multiboot2: GRUB sets eax == MULTIBOOT2_BOOTLOADER_MAGIC, rbx = info ptr
    mov     rax, [rsp+8]    ; get the return address pushed by the bootloader (multiboot2 puts magic in rax/eax at entry)
    cmp     eax, MULTIBOOT2_BOOTLOADER_MAGIC
    jne     .not_multiboot2

    ; --- Multiboot2 detected ---
    mov     rdi, rbx                  ; boot info pointer
    mov     esi, 2                    ; boot_type = 2 (Multiboot2)
    jmp     .enter_kernel

.not_multiboot2:
    ; UEFI or unknown (called by efi stub, etc.)
    xor     rdi, rdi                  ; boot info pointer = NULL or your EFI struct if available
    mov     esi, 1                    ; boot_type = 1 (UEFI)

.enter_kernel:
    call    kernel_main               ; void kernel_main(void* bootinfo, uint32_t boot_type)

.hang:
    cli
    hlt
    jmp .hang

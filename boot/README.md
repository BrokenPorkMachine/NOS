# nboot Agent (NitrOS)

**=nboot** is the UEFI bootloader for the NitrOS operating system.  
It loads the Nitrous kernel and NOSM modules, validates signatures, and passes a manifest-rich bootinfo to the kernel at startup.

## Features
- UEFI compliant, written in modern C
- Loads kernel and modules (`.nosm`) from disk/EFI partition
- Passes complete bootinfo to Nitrous (kernel, modules, memory map, ACPI, graphics)
- Supports secure boot and signature validation

## Build
- Install a UEFI toolchain (`gnu-efi`, `edk2`, or MinGW`)
- Build with CMake:
  ```sh
  mkdir build && cd build
  cmake ..
  make
  ```

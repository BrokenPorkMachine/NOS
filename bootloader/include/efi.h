#ifndef EFI_H
#define EFI_H

// ... all your typedefs and structs, as before ...

#ifndef EFIAPI
#define EFIAPI
#endif

VOID *EFIAPI CopyMem(VOID *Destination, const VOID *Source, UINTN Length);
VOID *EFIAPI SetMem(VOID *Buffer, UINTN Size, UINT8 Value);

#endif // EFI_H

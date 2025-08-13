.RECIPEPREFIX = >
CC=clang
LD=lld-link

CFLAGS=--target=x86_64-pc-win32-coff \
       -ffreestanding -fshort-wchar -mno-red-zone \
       -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables \
       -Wall -Wextra -Wno-unused-function \
       -Iinclude

LDFLAGS=/entry:efi_main \
        /subsystem:efi_application \
        /nodefaultlib \
        /base:0 \
        /filealign:32

all: nboot.efi nosformatu.bin

nboot.efi: nboot.obj
>$(LD) $(LDFLAGS) /out:nboot.efi nboot.obj

nosformatu.bin: nosformatu.obj
>$(LD) $(LDFLAGS) /out:nosformatu.bin nosformatu.obj

%.obj: %.c
>$(CC) $(CFLAGS) -c -o $@ $<

clean:
>rm -f *.obj *.efi *.bin

.PHONY: all clean

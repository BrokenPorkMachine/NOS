CC=clang
CFLAGS=-target x86_64-pc-win32-coff -ffreestanding -fshort-wchar -mno-red-zone \
       -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables \
       -Iinclude -Wall -Wextra -Werror

LDFLAGS=-fuse-ld=lld -nostdlib \
        -Wl,--entry:efi_main \
        -Wl,--subsystem,efi_application \
        -Wl,--image-base,0x100000 \
        -Wl,--file-alignment,0x20 \
        -Wl,--section-alignment,0x20

SRC=src/NitrOBoot.c
OBJ=src/NitrOBoot.o

all: NitrOBoot.efi

NitrOBoot.efi: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o *.efi

.PHONY: all clean

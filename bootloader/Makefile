CC=clang
CFLAGS=--target=x86_64-pc-win32-coff -ffreestanding -fshort-wchar \
       -mno-red-zone -fno-stack-protector -fno-exceptions \
       -fno-asynchronous-unwind-tables -Iinclude -Wall -Wextra -Werror

LDFLAGS=/entry:efi_main \
        /subsystem:efi_application \
        /align:32 \
        /filealign:32 \
        /base:0x100000 \
        /NODEFAULTLIB

SRC=src/NitrOBoot.c
OBJ=src/NitrOBoot.o

all: NitrOBoot.efi

NitrOBoot.efi: $(OBJ)
	lld-link $(LDFLAGS) /out:$@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o *.efi

.PHONY: all clean

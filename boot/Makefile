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
        /filealign:32 \
        /out:nboot.efi

# Build all C sources in this directory. The original build assumed sources
# lived under a `src/` subdirectory, but `nboot.c` now resides at the top level
# of `boot/`. Use a wildcard that matches files in the current directory so the
# boot agent links correctly.
SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.obj)

all: nboot.efi

nboot.efi: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ)

%.obj: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.obj *.efi

.PHONY: all clean

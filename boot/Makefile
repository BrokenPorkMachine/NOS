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
        /out:n nboot.efi

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.obj)

all: nboot.efi

nboot.efi: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ)

%.obj: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.obj *.efi

.PHONY: all clean

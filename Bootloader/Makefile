ARCH = x86_64
EFIINC = /usr/include/efi
EFILIB = /usr/lib
CFLAGS = -O2 -fpic -fshort-wchar -mno-red-zone -Wall -fstack-protector-strong \
  -D_FORTIFY_SOURCE=2 -I$(EFIINC) -I$(EFIINC)/$(ARCH) -DEFI_FUNCTION_WRAPPER
LDFLAGS = -nostdlib -znocombreloc -T $(EFIINC)/elf_$(ARCH)_efi.lds -shared -Bsymbolic


OBJS = main.o sha256.o

all: bootx64.efi

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

sha256.o: sha256.c sha256.h
	$(CC) $(CFLAGS) -c sha256.c -o $@

bootloader.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@

bootx64.efi: bootloader.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .reloc \
	--target=efi-app-$(ARCH) bootloader.so bootx64.efi

clean:
	rm -f *.o *.so *.efi

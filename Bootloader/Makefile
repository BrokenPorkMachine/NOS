ARCH = x86_64
EFIINC = /usr/include/efi
EFILIB = /usr/lib
CFLAGS = -O2 -fpic -fshort-wchar -mno-red-zone -Wall \
  -I$(EFIINC) -I$(EFIINC)/$(ARCH) -DEFI_FUNCTION_WRAPPER
LDFLAGS = -nostdlib -znocombreloc -T $(EFIINC)/elf_$(ARCH)_efi.lds -shared -Bsymbolic

all: bootx64.efi

bootloader.so: main.c
	$(CC) $(CFLAGS) -c $< -o main.o
	ld $(LDFLAGS) main.o -o bootloader.so

bootx64.efi: bootloader.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .reloc \
	--target=efi-app-$(ARCH) bootloader.so bootx64.efi

clean:
	rm -f *.o *.so *.efi

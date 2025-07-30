CC      = gcc
LD      = ld
ARCH    = x86_64
EFIINC  = /usr/include/efi
EFILIB  = /usr/lib
CFLAGS  = -O2 -fpic -fshort-wchar -Wall \
          -I$(EFIINC) -I$(EFIINC)/$(ARCH) -DEFI_FUNCTION_WRAPPER
LDFLAGS = -nostdlib -znocombreloc -T $(EFILIB)/elf_$(ARCH)_efi.lds -shared -Bsymbolic

all: bootx64.efi

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

bootloader.so: main.o
	$(LD) $(LDFLAGS) main.o -o bootloader.so

bootx64.efi: bootloader.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-x86_64 bootloader.so bootx64.efi

clean:
	rm -f *.o *.so *.efi

ARCH=x86_64
EFI_INCDIR=/usr/include/efi
EFI_LIBDIR=/usr/lib
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
OBJCOPY=$(CROSS_COMPILE)objcopy

CFLAGS=-I$(EFI_INCDIR) -I$(EFI_INCDIR)/$(ARCH) -fpic -fshort-wchar -mno-red-zone -fno-stack-protector -ffreestanding -Wall -Wextra
LDFLAGS=-T $(EFI_LIBDIR)/elf_$(ARCH)_efi.lds -nostdlib -znocombreloc -shared -Bsymbolic

TARGET=bootx64.efi
OBJS=BootX64.o

all: $(TARGET)

%.o: %.c
$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
$(LD) $(LDFLAGS) -o $@ $(EFI_LIBDIR)/crt0-efi-$(ARCH).o $(OBJS) -L$(EFI_LIBDIR) -lgnuefi -lefi
$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rela -j .rel -j .rel.* -j .rela.* -j .bss -j .rodata -j .eh_frame --target=efi-app-$(ARCH) $@

clean:
rm -f $(OBJS) $(TARGET)

.PHONY: all clean

# Makefile

TARGET = NitrOBoot.efi
OBJS = src/NitrOBoot.o

CFLAGS = \
	-target x86_64-pc-win32-coff \
	-ffreestanding \
	-fshort-wchar \
	-mno-red-zone \
	-fno-stack-protector \
	-fno-exceptions \
	-fno-asynchronous-unwind-tables \
	-Iinclude \
	-Wall \
	-Wextra \
	-c

LDFLAGS = \
	-target x86_64-pc-win32-coff \
	-nostdlib \
	-Wl,-entry:efi_main \
	-Wl,-subsystem:efi_application \
	-Wl,-dll \
	-fuse-ld=lld-link \
	-Wl,-nodefaultlib \
	-Wl,-align:4096 \
	-Wl,-filealign:4096

all: $(TARGET)

%.o: %.c
	clang $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	clang $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)

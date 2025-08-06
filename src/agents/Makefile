# src/agents/regx/Makefile

CC = clang
CFLAGS = -target x86_64-apple-macos10.7 -O2 -Wall -fno-builtin -nostdlib -fPIC

SRCS = regx.c regx_ipc.c
OBJS = $(SRCS:.c=.o)

regx.macho2: $(OBJS)
	$(CC) $(CFLAGS) -Wl,-e,_regx_main -o $@ $(OBJS)
	@echo "Mach-O2 agent built: $@"

clean:
	rm -f *.o regx.macho2

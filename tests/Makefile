CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11 \
    -I../kernel/IPC -I../kernel/Kernel -I../kernel/VM -I../boot/include \
    -I../user/servers/nitrfs -I../user/servers/login -I../user/libc \
    -I../kernel/drivers/IO
UNIT_TESTS=test_ipc test_pmm test_syscall test_nitrfs test_login

all: $(UNIT_TESTS)
	for t in $(UNIT_TESTS); do ./$$t; done

test_ipc: unit/test_ipc.c ../kernel/IPC/ipc.c ../user/libc/libc.c
	$(CC) $(CFLAGS) $^ -o $@

test_pmm: unit/test_pmm.c ../kernel/VM/pmm.c ../user/libc/libc.c
	$(CC) $(CFLAGS) $^ -o $@

test_syscall: unit/test_syscall.c ../kernel/Kernel/syscall.c ../kernel/Kernel/elf.c ../user/libc/libc.c
	$(CC) $(CFLAGS) $^ -o $@

test_nitrfs: unit/test_nitrfs.c ../user/servers/nitrfs/nitrfs.c ../user/libc/libc.c ../kernel/drivers/IO/block.c
	$(CC) $(CFLAGS) $^ -o $@

test_login: unit/test_login.c ../user/servers/login/login.c ../user/libc/libc.c ../kernel/IPC/ipc.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(UNIT_TESTS)

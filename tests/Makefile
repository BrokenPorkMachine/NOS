CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11 \
    -I../include -I../kernel/IPC -I../kernel/VM -I../boot/include \
    -I../user/agents/nosfs -I../user/agents/login -I../user/agents/ftp \
    -I../user/libc -I../kernel/drivers/IO -I../kernel/drivers/Audio \
    -I../kernel/drivers/Net -I../kernel/arch/GDT
LIBC_SRC=../user/libc/libc.c thread_stub.c
UNIT_TESTS=test_ipc test_pmm test_login test_ftp test_login_keyboard test_net test_gdt test_nosm test_nosfs

all: $(UNIT_TESTS)
	for t in $(UNIT_TESTS); do ./$$t; done

test_ipc: unit/test_ipc.c ../kernel/IPC/ipc.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_pmm: unit/test_pmm.c ../kernel/VM/pmm.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_login: unit/test_login.c ../user/agents/login/login.c $(LIBC_SRC) ../kernel/IPC/ipc.c ../kernel/agent.c
	$(CC) $(CFLAGS) $^ -o $@

test_login_keyboard: unit/test_login_keyboard.c ../user/agents/login/login.c \
../kernel/drivers/IO/tty.c $(LIBC_SRC) ../kernel/IPC/ipc.c ../kernel/agent.c
	$(CC) $(CFLAGS) $^ -o $@

test_ftp: unit/test_ftp.c ../user/agents/ftp/ftp.c ../kernel/IPC/ipc.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_net: unit/test_net.c ../kernel/drivers/Net/netstack.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_gdt: unit/test_gdt.c ../kernel/arch/GDT/gdt.c
	$(CC) $(CFLAGS) $^ -o $@

test_nosfs: unit/test_nosfs.c ../user/agents/nosfs/nosfs.c \
    ../kernel/drivers/IO/block.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_nosm: unit/test_nosm.c ../kernel/nosm.c ../kernel/agent.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(UNIT_TESTS)


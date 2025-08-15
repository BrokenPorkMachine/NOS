CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11 \
    -I../include -I../kernel -I../kernel/VM -I../boot/include \
    -I../user/agents/nosfs -I../user/agents/login -I../user/agents/ftp \
    -I../user/libc -I../nosm/drivers/IO -I../nosm/drivers/Audio \
    -I../nosm/drivers/Net -I../kernel/arch/GDT -I../src/agents/regx

LIBC_SRC=../user/libc/libc.c thread_stub.c smp_stub.c gdt_stub.c kprintf_stub.c vmm_stub.c ../kernel/uaccess.c

UNIT_TESTS=test_ipc test_pmm test_login test_ftp test_login_keyboard test_net test_gdt test_nosm test_nosfs test_regx test_thread test_nitroheap test_hal test_macho2 test_regx_load test_nh_classes test_nh_sys

all: $(UNIT_TESTS)
	for t in $(UNIT_TESTS); do ./$$t; done

test_ipc: unit/test_ipc.c ../kernel/IPC/ipc.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_pmm: unit/test_pmm.c ../kernel/VM/pmm.c ../kernel/VM/pmm_buddy.c ../kernel/VM/numa.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_login: unit/test_login.c ../user/agents/login/login.c $(LIBC_SRC) ../kernel/IPC/ipc.c ../kernel/agent.c
	$(CC) $(CFLAGS) -DLOGIN_UNIT_TEST $^ -o $@

test_login_keyboard: unit/test_login_keyboard.c ../user/agents/login/login.c $(LIBC_SRC) \
        ../kernel/IPC/ipc.c ../kernel/agent.c
	$(CC) $(CFLAGS) -DLOGIN_UNIT_TEST $^ -o $@

test_ftp: unit/test_ftp.c ../user/agents/ftp/ftp.c ../kernel/IPC/ipc.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_net: unit/test_net.c ../nosm/drivers/Net/netstack.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_gdt: unit/test_gdt.c ../kernel/arch/GDT/gdt.c $(filter-out gdt_stub.c,$(LIBC_SRC))
	$(CC) $(CFLAGS) $^ -o $@

test_nosfs: unit/test_nosfs.c ../user/agents/nosfs/nosfs.c \
    ../nosm/drivers/IO/block.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_nosm: unit/test_nosm.c ../kernel/IPC/ipc.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_regx: unit/test_regx.c ../kernel/regx.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_regx_load: unit/test_regx_load.c
	$(CC) $(CFLAGS) $^ -o $@

test_thread: unit/test_thread.c ../kernel/Task/thread.c thread_test_stubs.c $(filter-out thread_stub.c,$(LIBC_SRC))
	$(CC) $(CFLAGS) -DUNIT_TEST $^ -Wl,--gc-sections -o $@

test_nitroheap: unit/test_nitroheap.c ../kernel/VM/nitroheap/nitroheap.c \
        ../kernel/VM/nitroheap/classes.c buddy_stub.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_nh_classes: unit/test_nh_classes.c ../kernel/VM/nitroheap/classes.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_nh_sys: unit/test_nh_sys.c ../kernel/VM/nitroheap/nitroheap.c \
        ../kernel/VM/nitroheap/classes.c nh_sys_shim.c \
        buddy_stub.c $(filter-out ../user/libc/libc.c,$(LIBC_SRC))
	$(CC) $(CFLAGS) $^ -o $@

test_hal: unit/test_hal.c ../kernel/hal.c ../kernel/hal_async.c ../kernel/regx.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_macho2: unit/test_macho2.c ../kernel/macho2.c $(LIBC_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(UNIT_TESTS)


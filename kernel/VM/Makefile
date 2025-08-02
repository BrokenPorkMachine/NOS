paging.o: paging.c
	$(CC) $(CFLAGS) -c $< -o $@
# ...
kernel.bin: $(OBJS) paging.o kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) paging.o -o $@

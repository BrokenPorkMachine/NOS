# Compatibility Makefile for legacy build scripts
# Redirects builds to the top-level kernel target

.PHONY: all clean

all:
	$(MAKE) -C ../.. kernel CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	$(MAKE) -C ../.. clean


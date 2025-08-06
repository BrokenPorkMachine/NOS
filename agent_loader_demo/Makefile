CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -g
OBJS = agent_loader.o regx.o json.o demo.o

all: demo

demo: $(OBJS)
	$(CC) $(CFLAGS) -o demo $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) demo

.PHONY: all clean

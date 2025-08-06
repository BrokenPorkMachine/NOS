CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: demo

demo: demo.o agent_loader.o regx.o
	$(CC) $(CFLAGS) -o demo demo.o agent_loader.o regx.o

demo.o: demo.c agent_loader.h regx.h
agent_loader.o: agent_loader.c agent_loader.h regx.h
regx.o: regx.c regx.h

clean:
	rm -f *.o demo

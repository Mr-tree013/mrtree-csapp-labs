CC = gcc
CFLAGS = -Wall -O1 -m32

all: btest

btest: bits.c bits.h
	$(CC) $(CFLAGS) -o btest bits.c btest.c

clean:
	rm -f btest *.o

.PHONY: all clean

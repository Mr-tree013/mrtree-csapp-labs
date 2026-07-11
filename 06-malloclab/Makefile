CC = gcc
CFLAGS = -Wall -O2 -g -m64

all: mdriver

mdriver: mm.c mm.h
	$(CC) $(CFLAGS) -o mdriver mm.c mdriver.c

clean:
	rm -f mdriver *.o

.PHONY: all clean

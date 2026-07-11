CC = gcc
CFLAGS = -Wall -O2 -g

all: csim

csim: csim.c csim.h
	$(CC) $(CFLAGS) -o csim csim.c

clean:
	rm -f csim *.o

.PHONY: all clean

CC = gcc
CFLAGS = -Wall -O2 -g -pthread

all: proxy

proxy: proxy.c
	$(CC) $(CFLAGS) -o proxy proxy.c

clean:
	rm -f proxy *.o

.PHONY: all clean

CC = gcc
CFLAGS = -Wall -O2 -g

all: tsh

tsh: tsh.c
	$(CC) $(CFLAGS) -o tsh tsh.c

clean:
	rm -f tsh *.o

.PHONY: all clean

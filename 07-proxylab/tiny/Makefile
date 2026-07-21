CC = gcc
CFLAGS = -O2 -Wall -I .

# 此标志用于在 Linux 系统上链接 Pthreads 库。
# 其他系统可能需要不同的设置。
LIB = -lpthread

all: tiny cgi

tiny: tiny.c csapp.o
	$(CC) $(CFLAGS) -o tiny tiny.c csapp.o $(LIB)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

cgi:
	(cd cgi-bin; make)

clean:
	rm -f *.o tiny *~
	(cd cgi-bin; make clean)


# Proxy Lab 的 Makefile
#
# 你可以任意修改此文件（handin 规则除外）。
# 你的教师将使用你的 Makefile 通过 "make" 命令从源码构建你的 proxy。

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# 创建 ../proxylab-handin.tar 压缩包，可用于提交。请勿修改！
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz


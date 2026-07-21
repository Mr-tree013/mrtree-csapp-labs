/*
 * mysplit.c - 另一个用于测试你的微型shell的实用程序
 *
 * 用法: mysplit <n>
 * fork一个子进程，该子进程以1秒为间隔自旋<n>秒。
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

int main(int argc, char **argv)
{
    int i, secs;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <n>\n", argv[0]);
	exit(0);
    }
    secs = atoi(argv[1]);


    if (fork() == 0) { /* 子进程 */
	for (i=0; i < secs; i++)
	    sleep(1);
	exit(0);
    }

    /* 父进程等待子进程终止 */
    wait(NULL);

    exit(0);
}

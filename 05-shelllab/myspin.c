/*
 * myspin.c - 一个用于测试你的微型shell的实用程序
 *
 * 用法: myspin <n>
 * 以1秒为间隔睡眠<n>秒。
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int i, secs;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <n>\n", argv[0]);
	exit(0);
    }
    secs = atoi(argv[1]);
    for (i=0; i < secs; i++)
	sleep(1);
    exit(0);
}

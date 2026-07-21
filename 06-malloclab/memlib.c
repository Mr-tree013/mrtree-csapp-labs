/*
 * memlib.c - 一个模拟内存系统的模块。它的必要性在于
 *            可以让我们将学生 malloc 包的调用与
 *            系统中 libc 的 malloc 包调用交错进行。
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* 私有变量 */
static char *mem_start_brk;  /* 指向 heap 的第一个字节 */
static char *mem_brk;        /* 指向 heap 的最后一个字节 */
static char *mem_max_addr;   /* 最大合法 heap 地址 */

/*
 * mem_init - 初始化内存系统模型
 */
void mem_init(void)
{
    /* 分配用于模拟可用虚拟内存的存储空间 */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
	fprintf(stderr, "mem_init_vm: malloc error\n");
	exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 最大合法 heap 地址 */
    mem_brk = mem_start_brk;                  /* heap 初始为空 */
}

/*
 * mem_deinit - 释放内存系统模型使用的存储空间
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - 重置模拟的 brk 指针，使 heap 为空
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/*
 * mem_sbrk - sbrk 函数的简单模型。将 heap 扩展 incr 字节，
 *    并返回新区域的起始地址。在此模型中，heap 无法收缩。
 */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
	return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/*
 * mem_heap_lo - 返回 heap 第一个字节的地址
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/*
 * mem_heap_hi - 返回 heap 最后一个字节的地址
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - 返回 heap 的字节大小
 */
size_t mem_heapsize()
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - 返回系统的页面大小
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}

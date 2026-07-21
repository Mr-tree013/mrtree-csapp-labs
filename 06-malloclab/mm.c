/*
 * mm-naive.c - 最快、但内存利用率最低的 malloc 实现。
 *
 * 在这个简单方案中，分配一个 block 只需递增 brk 指针。
 * 一个 block 就是纯 payload，没有 header 或 footer。
 * block 永远不会被合并或重用。realloc 直接通过
 * mm_malloc 和 mm_free 实现。
 *
 * 致学生: 请用你自己的头注释替换此注释，
 * 在其中高层次地描述你的解决方案。
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 致学生: 在做任何其他事情之前，请先
 * 在下面的结构体中填写你团队的信息。
 ********************************************************/
team_t team = {
    /* 团队名称 */
    "ateam",
    /* 第一个成员的完整姓名 */
    "Harry Bovik",
    /* 第一个成员的邮箱地址 */
    "bovik@cs.cmu.edu",
    /* 第二个成员的完整姓名（如无则留空） */
    "",
    /* 第二个成员的邮箱地址（如无则留空） */
    ""
};

/* 单字（4 字节）或双字（8 字节）alignment */
#define ALIGNMENT 8

/* 向上取整到 ALIGNMENT 的最近倍数 */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - 初始化 malloc 包。
 */
int mm_init(void)
{
    return 0;
}

/*
 * mm_malloc - 通过递增 brk 指针来分配一个 block。
 *     始终分配大小为 alignment 倍数的 block。
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 释放 block 时不执行任何操作。
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - 直接使用 mm_malloc 和 mm_free 实现
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

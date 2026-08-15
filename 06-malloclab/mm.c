/*
 * mm.c - 基于「分离空闲链表」(segregated free list) 的动态内存分配器。
 *
 * 设计要点（32 位，-m32 编译）：
 *   1. 每个块有 4 字节的 header 和 4 字节的 footer（边界标记法 boundary tag），
 *      低 3 位因 8 字节对齐恒为 0，故用最低位作为「已分配」标志位。
 *   2. 空闲块通过 payload 里的前驱/后继指针，按大小分档挂到多个链表中
 *      （分离空闲链表），分配时按大小找到对应档位做 first-fit。
 *   3. 释放时立即与前后相邻空闲块合并（immediate coalescing），
 *      保证任何时刻都不存在相邻的两个空闲块。
 *   4. realloc 优先原地收缩/原地合并扩展，只有万不得已才搬运。
 *
 * 内部不变量（mm_check 会逐条校验）：
 *   - 所有块 8 字节对齐，且落在 heap 范围内；
 *   - header 与 footer 的 size/alloc 完全一致；
 *   - 不存在相邻的两个空闲块；
 *   - 每个空闲块都在且只在其对应大小档位的链表中；
 *   - 链表双向指针互相对应。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 团队信息（请替换为你自己的姓名和登录 ID）
 ********************************************************/
team_t team = {
    "ateam",
    "Harry Bovik",
    "bovik@cs.cmu.edu",
    "",
    ""
};

/* 调试开关：置 1 后会在每次 malloc/free/realloc 前后调用 mm_check 校验堆 */
#define DEBUG 0

/* ============================================================
 * 基础常量与宏
 * ============================================================ */

/* 对齐到 8 字节（单字 WSIZE=4，双字 DSIZE=8） */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE       4          /* 字 / header / footer 的大小（字节） */
#define DSIZE       8          /* 双字大小（字节） */
#define CHUNKSIZE   (1<<12)    /* 每次扩展堆的字节数（4096） */
#define MIN_BLOCK_SIZE 16      /* 最小块大小：header+footer+两个指针 = 16 */

/* 分离空闲链表的档位数 */
#define LIST_NUM 20

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*
 * 把一个块的大小和分配位打包成一个字。
 * size 的低 3 位恒为 0（因为 8 字节对齐），所以最低位可安全存放 alloc 位。
 */
#define PACK(size, alloc) ((size) | (alloc))

/* 读写地址 p 处的一个字（32 位无符号整数） */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* 从地址 p 处的字里取出大小（去掉低 3 位）和分配位（最低位） */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 给定块指针 bp（指向 payload 起始），计算 header / footer 地址 */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 给定块指针 bp，计算物理上相邻的前一个 / 后一个块的指针 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*
 * 空闲块内部用 payload 的前 8 字节存放两个指针：
 *   前驱指针在 bp 处，后继指针在 bp+WSIZE 处。
 * 这样空闲块在链表中是双向链接的。
 */
#define PRED_FREE(bp) (*(char **)(bp))
#define SUCC_FREE(bp) (*(char **)((char *)(bp) + WSIZE))

/* ============================================================
 * 全局变量
 * ============================================================ */

static char *heap_listp = NULL;   /* 指向序言块（prologue block） */

/*
 * 分离空闲链表：segregated_free_lists[i] 是第 i 档链表的表头。
 * 每个链表里的空闲块，其大小都落在 class_size[i] 定义的那一档。
 */
static char *segregated_free_lists[LIST_NUM];

/*
 * 各档链表的大小上界（class_size[i] 表示第 i 档能容纳的最大块大小）。
 * 即：块大小 size 落入「第一个满足 size <= class_size[i] 的档位 i」。
 * 越靠后的档位越大，最后一档用 (size_t)-1 表示「无穷大」，兜住所有超大块。
 */
static const size_t class_size[LIST_NUM] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
    16384, 32768, 65536, 131072, 262144, 524288, 1048576,
    2097152, 4194304, (size_t)-1
};

/* ============================================================
 * 函数原型
 * ============================================================ */

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static int   get_list_index(size_t size);
static void  insert_node(void *bp, size_t size);
static void  remove_node(void *bp);
static void  split_and_coalesce(void *bp, size_t asize);
static void  mm_check(void);

/* ============================================================
 * 对外接口：mm_init / mm_malloc / mm_free / mm_realloc
 * ============================================================ */

/*
 * mm_init - 初始化分配器。
 *
 * 1) 把分离空闲链表的所有表头置空；
 * 2) 建立初始堆结构：对齐填充 + 序言块(header/footer) + 结尾块(epilogue header)；
 * 3) 第一次扩展堆，得到一个初始空闲块。
 */
int mm_init(void)
{
    int i;

    /* 所有分离空闲链表初始为空 */
    for (i = 0; i < LIST_NUM; i++)
        segregated_free_lists[i] = NULL;

    /* 申请 4 个字，依次放：填充、序言块 header、序言块 footer、结尾块 header */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            /* 对齐填充（未使用） */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* 序言块 header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* 序言块 footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* 结尾块 header */

    /*
     * heap_listp 现在指向序言块（一个 8 字节、已分配的哑元块）。
     * 它作为遍历的起点，保证 PREV_BLKP 永远不会越过堆的起点。
     */
    heap_listp += (2 * WSIZE);

    /* 扩展空堆，得到一个 CHUNKSIZE 大小的初始空闲块 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    if (DEBUG) mm_check();
    return 0;
}

/*
 * mm_malloc - 分配一个 size 字节的块。
 *
 * 流程：调整请求大小（加上 header/footer 并对齐）→ 在空闲链表里找合适块
 *       → 找到则切分并返回；找不到则扩展堆 → 切分并返回。
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* 调整后的块大小（含 header/footer 且已对齐） */
    size_t extendsize; /* 需要向系统申请的字节数 */
    char *bp;

    if (size == 0)
        return NULL;

    /*
     * 计算所需块大小：
     *   块大小 = 8（header+footer）+ 向上对齐到 8 的 payload。
     * 公式 ALIGN(size + DSIZE) 即把 (size+8) 向上取整到 8 的倍数，
     * 最小也是 16 字节（满足 MIN_BLOCK_SIZE）。
     */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    /* 在空闲链表中寻找合适块 */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        if (DEBUG) mm_check();
        return bp;
    }

    /* 没有合适块：扩展堆（至少扩 CHUNKSIZE，若请求更大则按需扩） */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    if (DEBUG) mm_check();
    return bp;
}

/*
 * mm_free - 释放 ptr 指向的块，并与前后相邻空闲块合并。
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    /* 把块标记为空闲 */
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    /* 与相邻空闲块合并（内部会把它挂回合适的链表） */
    coalesce(ptr);

    if (DEBUG) mm_check();
}

/*
 * mm_realloc - 把 ptr 指向的块调整为 size 字节。
 *
 * 三种情况：
 *   1) 新尺寸 <= 旧块：原地收缩，多余部分切出去；
 *   2) 新尺寸更大，但下一个块空闲且够用：合并下一个块原地扩展；
 *   3) 否则：新分配一块 → 拷贝旧数据 → 释放旧块。
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t oldsize, asize;

    /* 标准 realloc 语义：ptr 为 NULL 等价于 malloc；size 为 0 等价于 free */
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    oldsize = GET_SIZE(HDRP(oldptr));

    /* 情况 1：原地收缩（asize <= oldsize） */
    if (asize <= oldsize) {
        split_and_coalesce(oldptr, asize);
        if (DEBUG) mm_check();
        return oldptr;
    }

    /* 情况 2：与下一个空闲块合并，原地扩展 */
    {
        void *next = NEXT_BLKP(oldptr);
        size_t next_size = GET_SIZE(HDRP(next));

        if (!GET_ALLOC(HDRP(next)) && (oldsize + next_size >= asize)) {
            remove_node(next);
            PUT(HDRP(oldptr), PACK(oldsize + next_size, 1));
            PUT(FTRP(oldptr), PACK(oldsize + next_size, 1));
            /* 合并后可能仍有富余，切出去避免浪费 */
            split_and_coalesce(oldptr, asize);
            if (DEBUG) mm_check();
            return oldptr;
        }
    }

    /* 情况 3：无法原地扩展 → 新分配 + 拷贝 + 释放旧块 */
    if ((newptr = mm_malloc(size)) == NULL)
        return NULL;
    /* 旧块 payload 大小 = oldsize - DSIZE（去掉 header/footer） */
    memcpy(newptr, oldptr, (size < oldsize - DSIZE) ? size : (oldsize - DSIZE));
    mm_free(oldptr);
    if (DEBUG) mm_check();
    return newptr;
}

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/*
 * get_list_index - 把块大小映射到链表档位下标。
 * 返回第一个满足 size <= class_size[i] 的 i。
 */
static int get_list_index(size_t size)
{
    int i;
    for (i = 0; i < LIST_NUM; i++)
        if (size <= class_size[i])
            return i;
    return LIST_NUM - 1;
}

/*
 * insert_node - 把一个空闲块插入对应档位链表的表头（LIFO）。
 */
static void insert_node(void *bp, size_t size)
{
    int idx = get_list_index(size);
    void *head = segregated_free_lists[idx];

    /* 头插法：新块成为新表头 */
    PRED_FREE(bp) = NULL;
    SUCC_FREE(bp) = head;
    if (head != NULL)
        PRED_FREE(head) = bp;
    segregated_free_lists[idx] = bp;
}

/*
 * remove_node - 把一个空闲块从它所在的链表中摘除。
 * 块大小从它自己的 header 读出，据此确定它在哪个档位。
 */
static void remove_node(void *bp)
{
    int idx = get_list_index(GET_SIZE(HDRP(bp)));
    void *pred = PRED_FREE(bp);
    void *succ = SUCC_FREE(bp);

    /* 拼接前驱与后继，跳过 bp 自己 */
    if (pred != NULL)
        SUCC_FREE(pred) = succ;
    else
        segregated_free_lists[idx] = succ;  /* bp 是表头，表头后移 */

    if (succ != NULL)
        PRED_FREE(succ) = pred;
}

/*
 * coalesce - 与前后相邻空闲块合并。
 *
 * 这是「边界标记法」的核心收益：借助相邻块的 footer/header，只需 O(1)
 * 就能判断前后邻居是否空闲并合并，避免产生越来越多无法利用的碎片。
 */
static void *coalesce(void *bp)
{
    void *prev_bp = PREV_BLKP(bp);
    void *next_bp = NEXT_BLKP(bp);
    size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        /* 情况 1：前后都占用，无需合并，直接挂回链表 */
    }
    else if (prev_alloc && !next_alloc) {
        /* 情况 2：向后合并（与下一个空闲块） */
        size += GET_SIZE(HDRP(next_bp));
        remove_node(next_bp);
    }
    else if (!prev_alloc && next_alloc) {
        /* 情况 3：向前合并（与前一个空闲块） */
        size += GET_SIZE(HDRP(prev_bp));
        remove_node(prev_bp);
        bp = prev_bp;
    }
    else {
        /* 情况 4：前后都空闲，三者合并 */
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        remove_node(prev_bp);
        remove_node(next_bp);
        bp = prev_bp;
    }

    /* 统一更新合并后块的 header/footer，并挂回链表 */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_node(bp, size);

    return bp;
}

/*
 * extend_heap - 用 mem_sbrk 扩展堆，得到一个空闲块并合并。
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 保证 size 是 8 的倍数（words 为奇数时多补一个字） */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    /* 初始化新空闲块的 header/footer，并设置新的结尾块 header */
    PUT(HDRP(bp), PACK(size, 0));         /* 空闲块 header（覆盖旧结尾块） */
    PUT(FTRP(bp), PACK(size, 0));         /* 空闲块 footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 新的结尾块 header */

    /* 若前一个块也是空闲的，则与之合并 */
    return coalesce(bp);
}

/*
 * find_fit - 在分离空闲链表中寻找第一个能容纳 asize 的空闲块。
 * 从与 asize 匹配的档位开始向上找，档位内 first-fit。
 */
static void *find_fit(size_t asize)
{
    int idx = get_list_index(asize);
    int i;
    void *bp;

    for (i = idx; i < LIST_NUM; i++) {
        for (bp = segregated_free_lists[i]; bp != NULL; bp = SUCC_FREE(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
        }
    }
    return NULL;
}

/*
 * place - 把空闲块 bp 中 asize 字节标记为已分配，富余部分切分为新空闲块。
 * 前提：bp 已在某个空闲链表中（由 find_fit 找到），需先摘除。
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    remove_node(bp);   /* 先摘除，因为这块马上要变成已分配 */

    if ((csize - asize) >= MIN_BLOCK_SIZE) {
        /* 富余部分够大，切分：前面 asize 分配，后面 (csize-asize) 变空闲 */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        /*
         * 这里可以直接 insert_node 而不用 coalesce，原因：
         * 空闲链表维持「无相邻空闲块」不变量，bp 被找到时它的邻居都是
         * 已分配，切分后新空闲块的邻居仍是已分配，所以无需合并。
         */
        insert_node(bp, csize - asize);
    }
    else {
        /* 富余太小，不切分，整个块都标记为已分配（形成内部碎片） */
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * split_and_coalesce - 把已分配块 bp 收缩到 asize，并把尾部富余切分为
 * 空闲块、与后续可能存在的空闲块合并。供 realloc 原地收缩/扩展使用。
 */
static void split_and_coalesce(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    void *tail;

    /* 富余不足一个最小块，不值得切分，保留为内部碎片 */
    if (csize - asize < MIN_BLOCK_SIZE)
        return;

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    tail = NEXT_BLKP(bp);
    PUT(HDRP(tail), PACK(csize - asize, 0));
    PUT(FTRP(tail), PACK(csize - asize, 0));

    /* tail 后面的块可能也是空闲的，交给 coalesce 统一处理 */
    coalesce(tail);
}

/* ============================================================
 * 堆一致性检查（heap checker）
 * ============================================================ */

/*
 * mm_check - 校验堆的所有内部不变量，发现问题打印错误信息。
 * 仅在 DEBUG=1 时被调用，不影响正常打分。手动调试时也可直接调用。
 */
static void mm_check(void)
{
    char *bp;
    int free_in_heap = 0;   /* 遍历堆时数到的空闲块个数 */
    int free_in_lists = 0;  /* 遍历链表时数到的空闲块个数 */
    int i;

    /* 检查序言块 */
    if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp)))
        printf("mm_check: 序言块损坏\n");

    /* 逐块遍历，检查对齐、越界、header/footer 一致、无相邻空闲块 */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (((unsigned int)bp % ALIGNMENT) != 0)
            printf("mm_check: 块 %p 未对齐\n", bp);
        if (bp < (char *)mem_heap_lo() || bp > (char *)mem_heap_hi())
            printf("mm_check: 块 %p 越界\n", bp);
        if (GET(HDRP(bp)) != GET(FTRP(bp)))
            printf("mm_check: 块 %p 的 header/footer 不一致\n", bp);

        if (!GET_ALLOC(HDRP(bp))) {
            free_in_heap++;
            if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))))
                printf("mm_check: 块 %p 之后存在相邻空闲块（未合并）\n", bp);
        }
    }

    /* 检查结尾块（遍历结束时的 bp 应是结尾块） */
    if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
        printf("mm_check: 结尾块损坏\n");

    /* 检查分离空闲链表：链接正确、档位正确、数量一致 */
    for (i = 0; i < LIST_NUM; i++) {
        for (bp = segregated_free_lists[i]; bp != NULL; bp = SUCC_FREE(bp)) {
            free_in_lists++;
            if (get_list_index(GET_SIZE(HDRP(bp))) != i)
                printf("mm_check: 空闲块 %p 处于错误的档位\n", bp);
            if (SUCC_FREE(bp) != NULL && PRED_FREE(SUCC_FREE(bp)) != bp)
                printf("mm_check: 块 %p 的链表双向指针断裂\n", bp);
        }
    }

    if (free_in_heap != free_in_lists)
        printf("mm_check: 空闲块数量不一致（堆内 %d，链表 %d）\n",
               free_in_heap, free_in_lists);
}

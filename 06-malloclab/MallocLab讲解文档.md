# Malloc Lab 讲解文档

> 对应源码：`mm.c`。本实验实现一个动态内存分配器（`mm_malloc` / `mm_free` / `mm_realloc`），
> 在保证正确的前提下，最大化「内存利用率 + 吞吐量」的加权成绩。
> 本实现采用 **分离空闲链表（segregated free list）+ 边界标记合并 + 原地 realloc**。

---

## 一、这个实验在做什么

操作系统给用户程序提供了 `malloc` / `free` / `realloc`，它们的底层是 C 运行时库（libc）实现的动态内存分配器，负责在「堆」（heap）里划出一块块内存给程序用，用完了再回收复用。

这个实验让你**自己实现一个精简版分配器**：

- 堆不是真的 OS 堆，而是 `memlib.c` 用 `mem_sbrk(incr)` 模拟出来的（内部是一大块 `malloc` 出来的内存 + 一个 brk 指针）。
- 你只能通过 `mem_sbrk` 向系统"要内存"，通过 `mem_heap_lo()` / `mem_heap_hi()` 知道堆的边界。
- **绝不能调用系统的 `malloc` / `free` / `realloc`**（否则就是作弊，且会被驱动检测）。

评判标准：`mdriver` 用若干 trace 文件（记录一串 `a 分配` / `f 释放` 操作）驱动你的分配器，检查三件事：

1. **正确性（valid）**：每个返回的指针必须 8 字节对齐、落在堆内、且不能和其他存活块重叠。有一条不满足，该 trace 直接 0 分。
2. **内存利用率（util）**：峰值存活 payload ÷ 峰值堆大小，越高越好。
3. **吞吐量（throughput）**：每秒能处理的分配/释放操作数。

最终 `Perf index = 60% × util + 40% × throughput`（吞吐量有上限 600 Kops）。

---

## 二、32 位模式：一切的前提

Makefile 里编译命令是 `gcc -Wall -O2 -m32`。**`-m32` 意味着 `size_t` 和指针都是 4 字节**，这决定了整个块布局：

- 单字 `WSIZE = 4` 字节（正好是 header/footer 的大小，也是指针的大小）；
- 双字 `DSIZE = 8` 字节（对齐单位）。

如果你按 64 位去写（以为 header 是 8 字节），会立刻出错。这是 malloclab 和你之前 cachelab（64 位）最大的思维差异。

---

## 三、块的结构（block layout）

每个块在内存里的样子：

```
地址低 ──────────────────────────────► 地址高
┌──────────┬──────────────────┬──────────┐
│  header  │     payload      │  footer  │
│  4 字节  │   (用户数据区)    │  4 字节  │
└──────────┴──────────────────┴──────────┘
  ↑                               ↑
  bp - 4                       bp + size - 8
                                (即 bp - 4 是 header)
```

- **header 和 footer 各 4 字节**，存的是同一个字：`块大小(size) | 分配位(alloc)`。
- **payload** 是返回给用户的区域，`bp` 指向这里（`mm_malloc` 返回的就是 `bp`）。

关键性质：**块大小一定是 8 的倍数**（因为 payload 要对齐到 8，块总大小 = 8 + 对齐后的 payload，仍是 8 的倍数）。所以 size 的二进制**低 3 位恒为 0**，于是可以借最低位（LSB）存放"是否已分配"标志，而不影响 size 的真实值。

```c
#define PACK(size, alloc) ((size) | (alloc))   // 打包
#define GET_SIZE(p)       (GET(p) & ~0x7)      // 取大小：抹掉低 3 位
#define GET_ALLOC(p)      (GET(p) & 0x1)       // 取分配位：只看最低位
```

### 为什么要同时有 header 和 footer？（边界标记法 boundary tag）

footer 是给**向后合并**用的。想象你要释放一个块，想看看它**前面**那个块是不是也空闲，从而合并：

- 前面块的**大小**记录在它自己的 footer 里，而前一个块的 footer 恰好就在**当前块 header 往前 4 字节**处（`bp - 8`）。
- 于是 `PREV_BLKP(bp) = bp - GET_SIZE(bp - 8)`，**O(1) 就能跳到前一个块**。

没有 footer 的话，要向前遍历只能从头扫整个堆，合并会变成 O(n)。这就是"边界标记"的价值。

```c
#define HDRP(bp)       ((char *)(bp) - WSIZE)                          // 自己的 header
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     // 自己的 footer
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 下一个块
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 前一个块
```

### 一个容易绕晕但对齐正确的点

header 是 4 字节，块大小是 8 的倍数，所以：**所有块的 header 都落在 `4 mod 8` 的地址，payload 和 footer 都落在 `0 mod 8`（8 字节对齐）的地址**。也就是说 `mm_malloc` 返回的 `bp` 天然是 8 字节对齐的，恰好满足驱动的对齐检查。如果你画出序言块之后的第一个块，就能验证这个不变量。

---

## 四、分离空闲链表（segregated free list）

最朴素的分配器用一个隐式链表：每次 `find_fit` 都要从头遍历所有块（含已分配的），速度慢、且容易把小块塞进大洞里造成浪费。

**分离空闲链表**把空闲块按大小分成多"档"，每档一条链表，分配时**直接跳到合适的那一档去找**，又快又省：

```c
#define LIST_NUM 20
static const size_t class_size[LIST_NUM] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
    16384, 32768, 65536, 131072, 262144, 524288, 1048576,
    2097152, 4194304, (size_t)-1   // 最后一档 (size_t)-1 表示"无穷大"，兜底
};
```

- `class_size[i]` 是第 i 档能容纳的**最大**块大小。
- 一个空闲块大小 `size` 归入**第一个满足 `size <= class_size[i]` 的档位**（`get_list_index` 做的事）。
- 分配 `asize` 时，从与它匹配的档位开始**向上**找，档内 first-fit（`find_fit`）。

空闲块之间用**双向链表**连接，前驱/后继指针就存在空闲块的 payload 前 8 字节里（反正空闲时这 8 字节没人用）：

```c
#define PRED_FREE(bp) (*(char **)(bp))                  // 前驱指针在 bp
#define SUCC_FREE(bp) (*(char **)((char *)(bp) + WSIZE)) // 后继指针在 bp+4
```

这也就是为什么**最小块是 16 字节**：header(4) + 两个指针(8) + footer(4) = 16，空闲块必须至少能放下这两个指针。

---

## 五、七个核心函数逐一讲解

### 5.1 `mm_init` —— 初始化

```c
int mm_init(void)
{
    for (i = 0; i < LIST_NUM; i++)
        segregated_free_lists[i] = NULL;          // ① 链表全空

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            // ② 对齐填充
    PUT(heap_listp + 1*WSIZE, PACK(DSIZE, 1));     // ② 序言块 header
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));     // ② 序言块 footer
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));         // ② 结尾块 header
    heap_listp += 2*WSIZE;                          // ③ 指向序言块

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)     // ④ 得到初始空闲块
        return -1;
    return 0;
}
```

两个哨兵块（sentinel）的作用：

- **序言块（prologue）**：一个 8 字节、已分配的哑元块，放在堆的最开头。它保证 `PREV_BLKP` 永远有地方可指，遍历时不会越到堆外。
- **结尾块（epilogue）**：一个 size=0、已分配的哑元块，永远在堆末尾。遍历时 `GET_SIZE == 0` 就是"到结尾了"的终止条件。

### 5.2 `mm_malloc` —— 分配

```c
void *mm_malloc(size_t size)
{
    if (size == 0) return NULL;

    if (size <= DSIZE) asize = 2 * DSIZE;           // 最小块 16
    else               asize = ALIGN(size + DSIZE); // 块 = 8(头尾) + 对齐后的 payload

    if ((bp = find_fit(asize)) != NULL) {           // ① 找到合适空闲块
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);             // ② 没找到 → 扩堆
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
```

**`asize` 的计算是易错点**：用户要 `size` 字节 payload，但块还要包含 4 字节 header 和 4 字节 footer，且 payload 要对齐到 8。所以：

```
块大小 asize = ALIGN(size + 8)
```

其中 `ALIGN(x)` 把 `x` 向上取整到 8 的倍数。举例：`size=1` → `ALIGN(9)=16`；`size=48` → `ALIGN(56)=56`；`size=2040` → `ALIGN(2048)=2048`。

### 5.3 `find_fit` 与 `place` —— 找块与切分

```c
static void *find_fit(size_t asize)
{
    int idx = get_list_index(asize);
    for (i = idx; i < LIST_NUM; i++)                // 从匹配档位向上找
        for (bp = segregated_free_lists[i]; bp; bp = SUCC_FREE(bp))
            if (GET_SIZE(HDRP(bp)) >= asize)        // 档内 first-fit
                return bp;
    return NULL;
}
```

```c
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_node(bp);                                 // 先摘除（马上要变已分配）

    if ((csize - asize) >= MIN_BLOCK_SIZE) {         // 富余够大 → 切分
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_node(bp, csize - asize);              // 尾部挂回链表
    } else {                                         // 富余太小 → 整块分配
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
```

**为什么切分后直接 `insert_node` 而不用 `coalesce`？** 因为空闲链表维持"无相邻空闲块"的不变量——`bp` 被找到时它的前后邻居都是已分配的，切分后新空闲块的邻居仍是已分配，自然无需合并。

**为什么富余 < 16 就不切？** 切出一个不足最小块的空闲块，它连两个指针都放不下，反而破坏链表。所以干脆整个分配，多出的几字节算"内部碎片"。

### 5.4 `mm_free` 与 `coalesce` —— 释放与合并

```c
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));   // 标记空闲
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);                   // 与邻居合并
}
```

`coalesce` 是分配器的灵魂，四种情况（前/后邻居是否空闲）：

```c
static void *coalesce(void *bp)
{
    void *prev_bp = PREV_BLKP(bp), *next_bp = NEXT_BLKP(bp);
    size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));   // 看前块 footer
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));   // 看后块 header
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {          /* ① 前后都占用：不合并 */ }
    else if (prev_alloc && !next_alloc) {    /* ② 只和后块合并 */
        size += GET_SIZE(HDRP(next_bp));
        remove_node(next_bp);
    }
    else if (!prev_alloc && next_alloc) {    /* ③ 只和前块合并 */
        size += GET_SIZE(HDRP(prev_bp));
        remove_node(prev_bp);
        bp = prev_bp;
    }
    else {                                   /* ④ 前后都空闲：三者合并 */
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        remove_node(prev_bp);
        remove_node(next_bp);
        bp = prev_bp;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_node(bp, size);
    return bp;
}
```

要点：合并时要把被合并的邻居**先从链表摘除**（`remove_node`），否则会出现同一个块在链表里出现两次的 bug。

### 5.5 `extend_heap` —— 扩堆

```c
static void *extend_heap(size_t words)
{
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;  // 凑成 8 的倍数
    if ((bp = mem_sbrk(size)) == (void *)-1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));          // 新空闲块 header（覆盖旧结尾块）
    PUT(FTRP(bp), PACK(size, 0));          // 新空闲块 footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 新的结尾块 header

    return coalesce(bp);                   // 若前块空闲则合并
}
```

注意：`HDRP(bp)` 的位置恰好是**旧的结尾块 header**（因为新块紧跟在旧结尾块后面），所以那句 `PUT` 就是在"旧的结尾块位置"上写新空闲块的 header，逻辑上是连贯的。

### 5.6 `mm_realloc` —— 重分配（含两个原地优化）

标准 `realloc` 语义：`ptr==NULL` 等价 `malloc`，`size==0` 等价 `free`。除此之外尽量**原地**操作，避免搬数据：

1. **原地收缩**（`asize <= oldsize`）：把多余部分切出去（`split_and_coalesce`），返回原指针。
2. **原地扩展**（`asize > oldsize` 但下一个块空闲且够用）：与下一个块合并，若还有富余再切出去，返回原指针。
3. **万不得已才搬**：`malloc` 新块 → `memcpy` 旧数据 → `free` 旧块。

```c
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }

    asize = (size <= DSIZE) ? 2*DSIZE : ALIGN(size + DSIZE);
    oldsize = GET_SIZE(HDRP(oldptr));

    if (asize <= oldsize) {                    // ① 收缩
        split_and_coalesce(oldptr, asize);
        return oldptr;
    }

    next = NEXT_BLKP(oldptr);
    next_size = GET_SIZE(HDRP(next));
    if (!GET_ALLOC(HDRP(next)) && oldsize + next_size >= asize) {  // ② 原地扩展
        remove_node(next);
        PUT(HDRP(oldptr), PACK(oldsize + next_size, 1));
        PUT(FTRP(oldptr), PACK(oldsize + next_size, 1));
        split_and_coalesce(oldptr, asize);
        return oldptr;
    }

    newptr = mm_malloc(size);                  // ③ 搬运
    if (newptr == NULL) return NULL;
    memcpy(newptr, oldptr, min(size, oldsize - DSIZE));
    mm_free(oldptr);
    return newptr;
}
```

**拷贝长度**：旧块 payload 大小是 `oldsize - DSIZE`（去掉 header/footer 共 8 字节），拷贝 `min(size, oldsize-8)` 字节即可。

`split_and_coalesce` 是 `place` 的"收缩版"：把已分配块缩到 `asize`，尾部富余切为空闲块并交给 `coalesce`（因为尾部的后邻居可能也是空闲的，需要合并）。

### 5.7 `mm_check` —— 堆一致性检查器

这是 CLAUDE.md 强烈建议的：一个能主动暴露 bug 的检查器。它逐条校验内部不变量：

1. 序言块 / 结尾块是否完好；
2. 每个块是否对齐、是否越界；
3. header 与 footer 的 size/alloc 是否**完全一致**；
4. 是否存在相邻的两个空闲块（说明有块没合并）；
5. 每个空闲块是否在**正确档位**的链表里、双向指针是否互相咬合；
6. 遍历堆数到的空闲块数 == 遍历链表数到的空闲块数。

它通过 `#define DEBUG 0` 控制：置 1 后在每次 malloc/free/realloc 后自动调用。开发时打开它，能第一时间抓到"链表断裂""漏合并"这类隐蔽 bug。

---

## 六、核心不变量（判断实现对错的总纲）

1. 所有块 8 字节对齐，且落在 `[mem_heap_lo(), mem_heap_hi()]` 内。
2. header 与 footer 内容一致（size 和 alloc 位相同）。
3. **任意两个物理相邻的块，不会同时空闲**（合并必须彻底）。
4. 每个空闲块都在且只在**一个**链表里，且所在链表与它的 size 匹配。
5. 链表双向指针互相一致：`SUCC(p)` 的前驱是 `p`，`PRED(p)` 的后继是 `p`。
6. 分配时先摘除、释放时先合并再挂回——绝不让一个块在链表里出现两次或漏掉。

---

## 七、几个必须理解的概念

### 7.1 内部碎片 vs 外部碎片

- **内部碎片（internal fragmentation）**：分配出去的块比实际需要的 payload 大（比如 `place` 里富余 < 16 不切分，或 `asize` 对齐造成的浪费）。浪费在"已分配块内部"。
- **外部碎片（external fragmentation）**：空闲内存总量够，但被切成许多小块，**没有单个块能满足**一次较大分配。这是分离链表 + first-fit 也难免的，`short1-bal.rep` 的 66% 利用率主要就是它造成的。

### 7.2 为什么分离链表比隐式链表好

- **快**：`find_fit` 不用遍历所有已分配块，直接跳到合适档位。
- **省**：小块不会去"霸占"大块（因为大块在更高的档位里），降低内部碎片。
- **first-fit + LIFO**：实现简单，且实测吞吐量高（新释放的块在表头，很可能马上又被需要）。

### 7.3 合并的时机

本实现用**立即合并（immediate coalescing）**：每次 `free` 立刻合并。好处是实现简单、碎片少；代价是"刚合并又马上切分"的抖动（尤其对反复 malloc/free 相同大小的负载）。另一种是**延迟合并（deferred coalescing）**：先不合并，等需要大块时再统一合并，能减少抖动但实现复杂。本 lab 用立即合并已足够。

---

## 八、测试方法

```bash
make                                  # 编译（-m32）
./mdriver -V -f short1-bal.rep        # 跑一个 trace，-V 打印详情
./mdriver -V -f short2-bal.rep
./mdriver -h                          # 查看所有选项
```

调试内存相关 bug 时，把 `mm.c` 里的 `#define DEBUG 0` 改成 `1`，重新 `make`，`mm_check` 会在每次操作后自动校验堆并打印第一处违规。

**本机只有 `short1`/`short2` 两个 trace**，完整 11 个 trace（`amptjp`、`cccp`、`binary`、`realloc` 等）在 `config.h` 的 `TRACEDIR` 指向 CMU 服务器，本地没有，需自行下载后放到同一目录并更新路径。

**成绩对照**：naive 起点在 short1 上 50% util（70/100），本实现提升到 short1=66%（80/100）、short2=89%（94/100）。

---

## 九、建议的复盘问题

1. 把 `coalesce` 里 `remove_node` 那几行删掉会怎样？`mm_check` 能抓住吗？
2. 为什么 `place` 切分后尾部用 `insert_node`，而 `split_and_coalesce` 用 `coalesce`？两者能不能互换？
3. 如果把 `GET_SIZE(p)` 的 `~0x7` 改成 `~0x3`（只抹低 2 位）会出什么问题？
4. 为什么最小块是 16 字节？如果把它改成 8 字节，分离链表（需要存两个指针）还能工作吗？
5. `PREV_BLKP` 依赖 footer 才能 O(1) 找到前一块。如果去掉 footer（只留 header），向后合并会退化成什么复杂度？对性能有什么影响？
6. `find_fit` 从匹配档位**向上**找，而不是从最小档位开始，为什么更快？这样做会不会牺牲利用率？

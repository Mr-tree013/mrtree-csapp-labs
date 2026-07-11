# Malloc Lab

实现一个动态内存分配器（`malloc`、`free`、`realloc`），优化吞吐量和内存利用率。

## 目标

- 理解虚拟内存与堆管理
- 实现隐式或显式空闲链表分配器
- 理解碎片（内部碎片和外部碎片）
- 掌握 segregated free list 等优化策略

## 实现函数

- `mm_init()` — 初始化分配器
- `mm_malloc(size_t size)` — 分配内存
- `mm_free(void *ptr)` — 释放内存
- `mm_realloc(void *ptr, size_t size)` — 重新分配

## 约束

- 不能调用系统的 `malloc`/`free`/`realloc`
- 使用 `mem_sbrk` 扩展堆空间
- 必须在 64 位对齐约束下工作

## 文件

- `mm.h` — 分配器头文件与宏定义
- `mm.c` — 分配器实现

## 参考资料

- CS:APP3e 第 9.9 节
- [Malloc Lab 说明](http://csapp.cs.cmu.edu/3e/malloclab.pdf)

/*
 * cachelab.h - Cache Lab 辅助函数的原型声明
 */

#ifndef CACHELAB_TOOLS_H
#define CACHELAB_TOOLS_H

#define MAX_TRANS_FUNCS 100

typedef struct trans_func{
  void (*func_ptr)(int M,int N,int[N][M],int[M][N]);
  char* description;
  char correct;
  unsigned int num_hits;
  unsigned int num_misses;
  unsigned int num_evictions;
} trans_func_t;

/*
 * printSummary - 此函数为你的缓存模拟器提供了一种标准方式来显示
 * 最终的 hit 和 miss 统计信息
 */
void printSummary(int hits,  /* hit 次数 */
				  int misses, /* miss 次数 */
				  int evictions); /* eviction 次数 */

/* 用数据填充矩阵 */
void initMatrix(int M, int N, int A[N][M], int B[M][N]);

/* 产生正确结果的基准转置函数。 */
void correctTrans(int M, int N, int A[N][M], int B[M][N]);

/* 将给定的函数添加到函数列表中 */
void registerTransFunction(
    void (*trans)(int M,int N,int[N][M],int[M][N]), char* desc);

#endif /* CACHELAB_TOOLS_H */

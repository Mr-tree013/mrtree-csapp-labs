/*
 * trans.c - 矩阵转置 B = A^T
 *
 * 每个转置函数必须具有以下形式的原型：
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * 转置函数的评估方式是在一个 1KB 直接映射缓存（block 大小为 32 字节）
 * 上统计 miss 次数。
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - 这是你在作业 Part B 中将被评分的转置函数。
 *     请勿修改描述字符串 "Transpose submission"，因为驱动程序
 *     会搜索该字符串来识别待评分的转置函数。
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
}

/*
 * 你可以在下面定义额外的转置函数。为了帮助你入门，
 * 我们已经在下面定义了一个简单的示例。
 */

/*
 * trans - 一个简单的基准转置函数，未针对缓存进行优化。
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - 此函数向驱动程序注册你的转置函数。
 *     在运行时，驱动程序将评估每个已注册的函数并汇总它们的
 *     性能。这是尝试不同转置策略的便捷方式。
 */
void registerFunctions()
{
    /* 注册你的解答函数 */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* 注册任何额外的转置函数 */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - 此辅助函数检查 B 是否是 A 的转置。
 *     你可以在从转置函数返回之前调用它来检查转置的正确性。
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


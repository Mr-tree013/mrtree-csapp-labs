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
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    int i, j, k, l;

    if (M == 32 && N == 32) {
        /*
         * 32×32 —— 8×8 分块 + 行缓冲
         *
         * 缓存几何:32 个 set,每条缓存行 8 个 int(32 字节),矩阵一行
         * (32 int)占 4 条缓存行。set 下标 = (4*行号 + 列块号) % 32,
         * 因此"相隔 8 行"才会撞回同一组 set(4×8=32)。8×8 块的 8 行
         * 正好铺满 32 个 set、互不冲突 → 用 8×8 分块最优。
         *
         * 对角线块(A、B 落到同一组 set)的坑:先把 A 的一整行
         * (8 个连续 int = 1 条缓存行)读进 8 个局部变量,再写 B 对应的一列。
         * 这样即使 B 的写踢掉 A 的缓存行,值也已经安全躺在寄存器/栈里
         * (栈访问被 test-trans 的 trace 过滤器忽略,不计 miss)。
         */
        for (int i = 0; i < N; i += 8) {
            for (int j = 0; j < M; j += 8) {
                for (int p = 0; p < 8; p++) {
                    t0 = A[i+p][j+0]; t1 = A[i+p][j+1];
                    t2 = A[i+p][j+2]; t3 = A[i+p][j+3];
                    t4 = A[i+p][j+4]; t5 = A[i+p][j+5];
                    t6 = A[i+p][j+6]; t7 = A[i+p][j+7];
                    B[j+0][i+p] = t0; B[j+1][i+p] = t1;
                    B[j+2][i+p] = t2; B[j+3][i+p] = t3;
                    B[j+4][i+p] = t4; B[j+5][i+p] = t5;
                    B[j+6][i+p] = t6; B[j+7][i+p] = t7;
                }
            }
        }
    } else if (M == 64 && N == 64) {
    /*
     * 64×64 —— 8×8 外块 + 4×4 子块,三段式(对角交换)
     *
     * 缓存几何:一行 64 个 int = 8 条缓存行,set = (8*行号 + 列块号) % 32,
     * 因此"相隔 4 行"就撞 set(8×4=32)。一个 8×8 块的 8 行只落在 4 个
     * set 上(每 2 行撞一个),不能同时驻留缓存 → 不能像 32×32 那样整块
     * 处理。于是把 8×8 块拆成 4 个 4×4 子块,分三段:
     *   ① upper half:正常转置左上 4×4;右上 4×4 先"错位"暂存到
     *     B[j..j+3][i+4..i+7](它最终该去 B[j+4..j+7][i..i+3])。
     *   ② exchange:同时读"左下 A"和"错位暂存的右上",各归其位。
     *   ③ lower-right:正常转置右下 4×4。
     * 关键在②:把"读 A"和"写 B"错开,对角线块上 A/B 不再争抢同一组
     * set —— 这正是 64×64 从 1700 压到 1180 的原因。
     */
    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 64; j += 8) {

            /* 第 1 段:上半。k 遍历块的上 4 行(i..i+3):
             * a0~a3 = 左半 4 列 → 写 B 正确位置 B[j..j+3][k];
             * a4~a7 = 右半 4 列 → 先错位暂存到 B[j..j+3][k+4]。 */
            for (k = i; k < i + 4; ++k) {
                a0 = A[k][j];
                a1 = A[k][j + 1];
                a2 = A[k][j + 2];
                a3 = A[k][j + 3];
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];

                B[j][k]     = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;

                B[j][k + 4]     = a4;
                B[j + 1][k + 4] = a5;
                B[j + 2][k + 4] = a6;
                B[j + 3][k + 4] = a7;
            }

            /* 第 2 段:交换右上和左下。k 遍历 j..j+3:
             * a0~a3 = 左下 A[i+4..i+7][k];
             * a4~a7 = 第 1 段暂存的右上(位于 B[k][i+4..i+7]);
             * 先读走 a4~a7,再把左下写回 B[k][i+4..i+7],
             * 最后把右上搬到正确位置 B[k+4][i..i+3]。 */
            for (k = j; k < j + 4; ++k) {
                a0 = A[i + 4][k];
                a1 = A[i + 5][k];
                a2 = A[i + 6][k];
                a3 = A[i + 7][k];

                a4 = B[k][i + 4];
                a5 = B[k][i + 5];
                a6 = B[k][i + 6];
                a7 = B[k][i + 7];

                B[k][i + 4] = a0;
                B[k][i + 5] = a1;
                B[k][i + 6] = a2;
                B[k][i + 7] = a3;

                B[k + 4][i]     = a4;
                B[k + 4][i + 1] = a5;
                B[k + 4][i + 2] = a6;
                B[k + 4][i + 3] = a7;
            }

            /* 第 3 段:右下。k 遍历块的下 4 行(i+4..i+7),
             * 正常转置右下 4×4 子块。 */
            for (k = i + 4; k < i + 8; ++k) {
                a0 = A[k][j + 4];
                a1 = A[k][j + 5];
                a2 = A[k][j + 6];
                a3 = A[k][j + 7];

                B[j + 4][k] = a0;
                B[j + 5][k] = a1;
                B[j + 6][k] = a2;
                B[j + 7][k] = a3;
            }
        }
    }
    } else {
        /*
         * 61×67(及任意非 2 的幂尺寸)—— 16×16 分块 + 边界处理
         *
         * 关键:61、67 不是 2 的幂,行距 61*4、67*4 字节都不是 32 的
         * 倍数,set 映射被"打散",几乎没有规律性的冲突不命中——反而比
         * 32/64 简单。普通 16×16 分块即可,靠 k<N、l<M 处理最后不满 16
         * 的边角行/列。(B[l][k] = A[k][l]:k 是 A 的行、l 是 A 的列。)
         */
        for (i = 0; i < N; i += 16) {
            for (j = 0; j < M; j += 16) {
                for (k = i; k < i + 16 && k < N; ++k) {
                    for (l = j; l < j + 16 && l < M; ++l) {
                        B[l][k] = A[k][l];
                    }
                }
            }
        }
    }
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


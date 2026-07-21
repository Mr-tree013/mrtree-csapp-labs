/*
 * tracegen.c - 使用 valgrind 运行二进制 tracegen 可生成
 * 所有已注册转置函数的内存 trace。
 *
 * 每个已注册转置函数 trace 的开始和结束通过读取 "marker" 地址
 * 来标识。这两个 marker 地址被记录在文件中以供后续使用。
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include "cachelab.h"
#include <string.h>

/* 在 cachelab.c 中声明的外部变量 */
extern trans_func_t func_list[MAX_TRANS_FUNCS];
extern int func_counter;

/* 来自 trans.c 的外部函数 */
extern void registerFunctions();

/* 用于界定感兴趣的 trace 区域的标记 */
volatile char MARKER_START, MARKER_END;

static int A[256][256];
static int B[256][256];
static int M;
static int N;


int validate(int fn,int M, int N, int A[N][M], int B[M][N]) {
    int C[M][N];
    memset(C,0,sizeof(C));
    correctTrans(M,N,A,C);
    for(int i=0;i<M;i++) {
        for(int j=0;j<N;j++) {
            if(B[i][j]!=C[i][j]) {
                printf("Validation failed on function %d! Expected %d but got %d at B[%d][%d]\n",fn,C[i][j],B[i][j],i,j);
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char* argv[]){
    int i;

    char c;
    int selectedFunc=-1;
    while( (c=getopt(argc,argv,"M:N:F:")) != -1){
        switch(c){
        case 'M':
            M = atoi(optarg);
            break;
        case 'N':
            N = atoi(optarg);
            break;
        case 'F':
            selectedFunc = atoi(optarg);
            break;
        case '?':
        default:
            printf("./tracegen failed to parse its options.\n");
            exit(1);
        }
    }
  

    /* 注册转置函数 */
    registerFunctions();

    /* 用数据填充矩阵 A */
    initMatrix(M,N, A, B); 

    /* 记录 marker 地址 */
    FILE* marker_fp = fopen(".marker","w");
    assert(marker_fp);
    fprintf(marker_fp, "%llx %llx", 
            (unsigned long long int) &MARKER_START,
            (unsigned long long int) &MARKER_END );
    fclose(marker_fp);

    if (-1==selectedFunc) {
        /* 调用已注册的转置函数 */
        for (i=0; i < func_counter; i++) {
            MARKER_START = 33;
            (*func_list[i].func_ptr)(M, N, A, B);
            MARKER_END = 34;
            if (!validate(i,M,N,A,B))
                return i+1;
        }
    } else {
        MARKER_START = 33;
        (*func_list[selectedFunc].func_ptr)(M, N, A, B);
        MARKER_END = 34;
        if (!validate(selectedFunc,M,N,A,B))
            return selectedFunc+1;

    }
    return 0;
}



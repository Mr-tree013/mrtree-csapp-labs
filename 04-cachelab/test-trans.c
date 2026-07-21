/*
 * test-trans.c - 检查学生的所有转置函数的正确性和性能，
 *     并同时记录其正式提交版本的结果。
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include "cachelab.h"
#include <sys/wait.h> // 用于 WEXITSTATUS
#include <limits.h> // 用于 INT_MAX

/* 最大数组维度 */
#define MAXN 256

/* 学生提交以获得学分的 transpose_submit() 函数的描述字符串 */
#define SUBMIT_DESCRIPTION "Transpose submission"

/* 在 trans.c 中定义的外部函数 */
extern void registerFunctions();

/* 在 cachelab-tools.c 中定义的外部变量 */
extern trans_func_t func_list[MAX_TRANS_FUNCS];
extern int func_counter;

/* 通过命令行设置的全局变量 */
static int M = 0;
static int N = 0;

/* 提交的转置函数的正确性和性能 */
struct results {
    int funcid;
    int correct;
    int misses;
};
static struct results results = {-1, 0, INT_MAX};

/*
 * eval_perf - 评估已注册转置函数的性能
 */
void eval_perf(unsigned int s, unsigned int E, unsigned int b)
{
    int i,flag;
    unsigned int len, hits, misses, evictions;
    unsigned long long int marker_start, marker_end, addr;
    char buf[1000], cmd[255];
    char filename[128];

    registerFunctions(); 

    /* 打开完整的 trace 文件 */
    FILE* full_trace_fp;
    FILE* part_trace_fp;

    /* 评估每个已注册转置函数的性能 */

    for (i=0; i<func_counter; i++) {
        if (strcmp(func_list[i].description, SUBMIT_DESCRIPTION) == 0 )
            results.funcid = i; /* 记住哪个函数是提交的版本 */


        printf("\nFunction %d (%d total)\nStep 1: Validating and generating memory traces\n",i,func_counter);
        /* 使用 valgrind 生成 trace */

        sprintf(cmd, "valgrind --tool=lackey --trace-mem=yes --log-fd=1 -v ./tracegen -M %d -N %d -F %d  > trace.tmp", M, N,i);
        flag=WEXITSTATUS(system(cmd));
        if (0!=flag) {
            printf("Validation error at function %d! Run ./tracegen -M %d -N %d -F %d for details.\nSkipping performance evaluation for this function.\n",flag-1,M,N,i);      
            continue;
        }

        /* 获取起始和结束标记地址 */
        FILE* marker_fp = fopen(".marker", "r");
        assert(marker_fp);
        fscanf(marker_fp, "%llx %llx", &marker_start, &marker_end);
        fclose(marker_fp);


        func_list[i].correct=1;

        /* 保存转置提交的正确性 */
        if (results.funcid == i ) {
            results.correct = 1;
        }

        full_trace_fp = fopen("trace.tmp", "r");
        assert(full_trace_fp);


        /* 每个转置函数的过滤 trace 存入单独的文件 */
        sprintf(filename, "trace.f%d", i);
        part_trace_fp = fopen(filename, "w");
        assert(part_trace_fp);
    
        /* 定位与转置函数对应的 trace */
        flag = 0;
        while (fgets(buf, 1000, full_trace_fp) != NULL) {

            /* 我们只关心内存访问指令 */
            if (buf[0]==' ' && buf[2]==' ' &&
                (buf[1]=='S' || buf[1]=='M' || buf[1]=='L' )) {
                sscanf(buf+3, "%llx,%u", &addr, &len);
        
                /* 如果找到起始标记，设置标志 */
                if (addr == marker_start)
                    flag = 1;

                /* Valgrind 会产生许多与学生的代码无关的虚假
                   栈访问。目前，我们通过一个简单的过滤器忽略
                   所有栈访问，只记录地址空间低 32 位部分的访问。
                   将来如果有条件的话，可以尝试更智能的过滤方式，
                   以消除 valgrind 的栈引用，同时保留学生的栈引用。 */
                if (flag && addr < 0xffffffff) {
                    fputs(buf, part_trace_fp);
                }

                /* 如果找到结束标记，关闭 trace 文件 */
                if (addr == marker_end) {
                    flag = 0;
                    fclose(part_trace_fp);
                    break;
                }
            }
        }
        fclose(full_trace_fp);

        /* 运行参考模拟器 */
        printf("Step 2: Evaluating performance (s=%d, E=%d, b=%d)\n", s, E, b);
        char cmd[255];
        sprintf(cmd, "./csim-ref -s %u -E %u -b %u -t trace.f%d > /dev/null", 
                s, E, b, i);
        system(cmd);
    
        /* 从参考模拟器收集结果 */
        FILE* in_fp = fopen(".csim_results","r");
        assert(in_fp);
        fscanf(in_fp, "%u %u %u", &hits, &misses, &evictions);
        fclose(in_fp);
        func_list[i].num_hits = hits;
        func_list[i].num_misses = misses;
        func_list[i].num_evictions = evictions;
        printf("func %u (%s): hits:%u, misses:%u, evictions:%u\n",
               i, func_list[i].description, hits, misses, evictions);
    
        /* 如果是 transpose_submit()，记录 miss 次数 */
        if (results.funcid == i) {
            results.misses = misses;
        }
    }
  
}

/*
 * usage - 打印使用信息
 */
void usage(char *argv[]){
    printf("Usage: %s [-h] -M <rows> -N <cols>\n", argv[0]);
    printf("Options:\n");
    printf("  -h          Print this help message.\n");
    printf("  -M <rows>   Number of matrix rows (max %d)\n", MAXN);
    printf("  -N <cols>   Number of  matrix columns (max %d)\n", MAXN);
    printf("Example: %s -M 8 -N 8\n", argv[0]);       
}

/*
 * sigsegv_handler - SIGSEGV 处理函数
 */
void sigsegv_handler(int signum){
    printf("Error: Segmentation Fault.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

/*
 * sigalrm_handler - SIGALRM 处理函数
 */
void sigalrm_handler(int signum){
    printf("Error: Program timed out.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

/*
 * main - 主程序
 */
int main(int argc, char* argv[])
{
    char c;

    while ((c = getopt(argc,argv,"M:N:h")) != -1) {
        switch(c) {
        case 'M':
            M = atoi(optarg);
            break;
        case 'N':
            N = atoi(optarg);
            break;
        case 'h':
            usage(argv);
            exit(0);
        default:
            usage(argv);
            exit(1);
        }
    }
  
    if (M == 0 || N == 0) {
        printf("Error: Missing required argument\n");
        usage(argv);
        exit(1);
    }

    if (M > MAXN || N > MAXN) {
        printf("Error: M or N exceeds %d\n", MAXN);
        usage(argv);
        exit(1);
    }

    /* 安装 SIGSEGV 和 SIGALRM 信号处理函数 */
    if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to install SIGALRM handler\n");
        exit(1);
    }

    if (signal(SIGALRM, sigalrm_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to install SIGALRM handler\n");
        exit(1);
    }

    /* 超时后放弃 */
    alarm(120);

    /* 检查学生的转置函数的性能 */
    eval_perf(5, 1, 5);
  
    /* 输出本次测试的结果 */
    if (results.funcid == -1) {
        printf("\nError: We could not find your transpose_submit() function\n");
        printf("Error: Please ensure that description field is exactly \"%s\"\n", 
               SUBMIT_DESCRIPTION);
        printf("\nTEST_TRANS_RESULTS=0:0\n");
    }
    else {
        printf("\nSummary for official submission (func %d): correctness=%d misses=%d\n",
               results.funcid, results.correct, results.misses);
        printf("\nTEST_TRANS_RESULTS=%d:%d\n", results.correct, results.misses);
    }
    return 0;
}

/*
 * CS:APP Data Lab
 */

/* 声明不同的函数类型 */
typedef int (*funct_t) (void);
typedef int (*funct1_t)(int);
typedef int (*funct2_t)(int, int);
typedef int (*funct3_t)(int, int, int);

/* 将函数及其测试的所有信息组合为结构体 */
typedef struct {
    char *name;             /* 字符串名称 */
    funct_t solution_funct; /* 函数 */
    funct_t test_funct;     /* 测试函数 */
    int args;               /* 函数参数个数 */
    char *ops;              /* 合法运算符列表。特殊情况："$" 表示 floating point */
    int op_limit;           /* 解答中允许的最大 ops 数 */
    int rating;             /* 题目难度（1 -- 4） */
    int arg_ranges[3][2];   /* 参数范围。始终为 3 个参数定义，即使 */
                            /* 函数需要的参数更少。特殊情况：对于 f.p. 谜题，第一个参数 */
			    /* 必须设为 {1,1} */
} test_rec, *test_ptr;

extern test_rec test_set[];






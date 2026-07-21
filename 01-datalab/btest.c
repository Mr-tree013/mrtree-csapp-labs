/*
 * CS:APP Data Lab
 *
 * btest.c - 测试框架，检查学生在 bits.c 中的解答的正确性。
 *
 * 版权所有 (c) 2001-2011, R. Bryant 和 D. O'Hallaron，保留所有权利。
 * 未经许可，不得使用、修改或复制。
 *
 * 这是 btest 的改进版本，对整数谜题在零和 tmin/tmax 周围大范围测试，
 * 对 floating point 谜题在零、norm 和 denorm 边界周围测试。
 *
 * 注意：非 64 位安全。始终使用 gcc -m32 选项编译。
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <setjmp.h>
#include <math.h>
#include "btest.h"

/* 某些 stdlib.h 文件中未声明，因此在此定义 */
float strtof(const char *nptr, char **endptr);

/*************************
 * 配置常量
 *************************/

/* 通过设置执行时间上限（秒）来处理无限循环 */
#define TIMEOUT_LIMIT 10

/* 对于单参数函数，在最小和最大测试值上下以及零附近生成 TEST_RANGE 个测试值。
   具有两个或三个参数的函数将分别使用此值的平方根和立方根，
   以避免组合爆炸 */
#define TEST_RANGE 500000

/* 这定义了任何测试值数组的最大大小。gen_vals() 例程为
   TEST_RANGE 的每个值创建 k 个测试值，因此 MAX_TEST_VALS
   必须至少为 k*TEST_RANGE */
#define MAX_TEST_VALS 13*TEST_RANGE

/**********************************
 * 其他模块中定义的全局变量
 **********************************/
/* 这表征了要测试的谜题集合。
   在 decl.c 中定义，并从 ./puzzles 目录中的模板生成 */
extern test_rec test_set[];

/************************************************
 * 由命令行参数定义的仅写一次全局变量
 ************************************************/

/* 以自动评分格式输出结果，不显示反例 */
static int grade = 0;

/* 在此秒数后超时 */
static int timeout_limit = TIMEOUT_LIMIT; /* -T */

/* 如果非 NULL，仅测试一个函数（-f） */
static char* test_fname = NULL;

/* 当仅使用固定参数时的特殊情况（-1、-2 或 -3） */
static int has_arg[3] = {0,0,0};
static unsigned argval[3] = {0,0,0};

/* 对评分使用固定权重，如果是，则权重应为多少？（-r） */
static int global_rating = 0;

/******************
 * 辅助函数
 ******************/

/*
 * Signal - 安装信号处理函数
 */
typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* 阻塞正在处理的类型的信号 */
    action.sa_flags = SA_RESTART; /* 如有可能，重新启动系统调用 */

    if (sigaction(signum, &action, &old_action) < 0)
	perror("Signal error");
    return (old_action.sa_handler);
}

/*
 * timeout_handler - SIGALRM 处理函数
 */
sigjmp_buf envbuf;
void timeout_handler(int sig) {
    siglongjmp(envbuf, 1);
}

/*
 * random_val - 返回介于 min 和 max 之间的随机整数值
 */
static int random_val(int min, int max)
{
    double weight = rand()/(double) RAND_MAX;
    int result = min * (1-weight) + max * weight;
    return result;
}

/*
 * gen_vals - 生成我们将用于测试函数的整数值
 */
static int gen_vals(int test_vals[], int min, int max, int test_range, int arg)
{
    int i;
    int test_count = 0;

    /* 特殊情况：如果用户使用 -1、-2 或 -3 标志指定了特定的函数参数，
       则直接使用此参数并返回 */
    if (has_arg[arg]) {
	test_vals[0] = argval[arg];
	return 1;
    }

    /*
     * 特殊情况：为 floating point 函数生成测试值，
     * 其中输入参数是 float 的 unsigned 位级表示。
     * 对于这种情况，我们希望测试零附近、最小的 normalized 数和最大的
     * denormalized 数附近、1 附近以及最大的 normalized 数附近，
     * 以及 inf 和 nan。
     */
    if ((min == 1 && max == 1)) {
	unsigned smallest_norm = 0x00800000;
	unsigned one = 0x3f800000;
	unsigned largest_norm = 0x7f000000;

	unsigned inf = 0x7f800000;
	unsigned nan =  0x7fc00000;
	unsigned sign = 0x80000000;

	/* 测试范围应最多为一个指数值范围的一半 */
	if (test_range > (1 << 23)) {
	    test_range = 1 << 23;
	}

	/* 输入参数为 float 的 unsigned 位级表示的函数。
	   此循环体内生成的测试数量是全局变量
	   MAX_TEST_VALS 注释中引用的 k 值。 */

	for (i = 0; i < test_range; i++) {
	    /* 零周围的 denorms */
	    test_vals[test_count++] = i;
	    test_vals[test_count++] = sign | i;

	    /* norm 到 denorm 转换附近 */
	    test_vals[test_count++] = smallest_norm + i;
	    test_vals[test_count++] = smallest_norm - i;
	    test_vals[test_count++] = sign | (smallest_norm + i);
	    test_vals[test_count++] = sign | (smallest_norm - i);

	    /* 1 附近 */
	    test_vals[test_count++] = one + i;
	    test_vals[test_count++] = one - i;
	    test_vals[test_count++] = sign | (one + i);
	    test_vals[test_count++] = sign | (one - i);

	    /* 最大 norm 下方附近 */
	    test_vals[test_count++] = largest_norm - i;
	    test_vals[test_count++] = sign | (largest_norm - i);
	}

	/* 特殊值 */
	test_vals[test_count++] = inf;        /* inf */
	test_vals[test_count++] = sign | inf; /* -inf */
	test_vals[test_count++] = nan;        /* nan */
	test_vals[test_count++] = sign | nan; /* -nan */

	return test_count;
    }


    /*
     * 一般情况：为整数函数生成测试值
     */

    /* 如果范围足够小，则进行全面测试 */
    if (max - MAX_TEST_VALS <= min) {
	for (i = min; i <= max; i++)
	    test_vals[test_count++] = i;
	return test_count;
    }

    /* 否则，需要抽样。在边界附近、零附近以及一些随机情况进行测试。 */
    for (i = 0; i < test_range; i++) {

	/* 在边界附近测试 */
	test_vals[test_count++] = min + i;
	test_vals[test_count++] = max - i;

	/* 如果零落在 min 和 max 之间，则在零附近测试 */
	if (i >= min && i <= max)
	    test_vals[test_count++] = i;
	if (-i >= min && -i <= max)
	    test_vals[test_count++] = -i;

	/* min 和 max 之间的随机值 */
	test_vals[test_count++] = random_val(min, max);

    }
    return test_count;
}

/*
 * test_0_arg - 测试无参数的函数
 */
static int test_0_arg(funct_t f, funct_t ft, char *name)
{
    int r = f();
    int rt = ft();
    int error =  (r != rt);

    if (error && !grade)
	printf("ERROR: Test %s() failed...\n...Gives %d[0x%x]. Should be %d[0x%x]\n", name, r, r, rt, rt);

    return error;
}

/*
 * test_1_arg - 测试带一个参数的函数
 */
static int test_1_arg(funct_t f, funct_t ft, int arg1, char *name)
{
    funct1_t f1 = (funct1_t) f;
    funct1_t f1t = (funct1_t) ft;
    int r, rt, error;

    r = f1(arg1);
    rt = f1t(arg1);
    error = (r != rt);
    if (error && !grade)
	printf("ERROR: Test %s(%d[0x%x]) failed...\n...Gives %d[0x%x]. Should be %d[0x%x]\n", name, arg1, arg1, r, r, rt, rt);

    return error;
}

/*
 * test_2_arg - 测试带两个参数的函数
 */
static int test_2_arg(funct_t f, funct_t ft, int arg1, int arg2, char *name)
{
    funct2_t f2 = (funct2_t) f;
    funct2_t f2t = (funct2_t) ft;
    int r = f2(arg1, arg2);
    int rt = f2t(arg1, arg2);
    int error = (r != rt);

    if (error && !grade)
	printf("ERROR: Test %s(%d[0x%x],%d[0x%x]) failed...\n...Gives %d[0x%x]. Should be %d[0x%x]\n", name, arg1, arg1, arg2, arg2, r, r, rt, rt);

    return error;
}

/*
 * test_3_arg - 测试带三个参数的函数
 */
static int test_3_arg(funct_t f, funct_t ft,
		      int arg1, int arg2, int arg3, char *name)
{
    funct3_t f3 = (funct3_t) f;
    funct3_t f3t = (funct3_t) ft;
    int r = f3(arg1, arg2, arg3);
    int rt = f3t(arg1, arg2, arg3);
    int error = (r != rt);

    if (error && !grade)
	printf("ERROR: Test %s(%d[0x%x],%d[0x%x],%d[0x%x]) failed...\n...Gives %d[0x%x]. Should be %d[0x%x]\n", name, arg1, arg1, arg2, arg2, arg3, arg3, r, r, rt, rt);

    return error;
}

/*
 * test_function - 测试一个函数。返回错误数量
 */
static int test_function(test_ptr t) {
    int test_counts[3];    /* 每个参数的测试值数量 */
    int args = t->args;    /* 函数参数个数 */
    int arg_test_range[3]; /* 每个参数的测试范围 */
    int i, a1, a2, a3;
    int errors = 0;

    /* 这些是每个参数的测试值。使用 static 属性声明，
       以便数组分配在 bss 段而不是栈上 */
    static int arg_test_vals[3][MAX_TEST_VALS];

    /* 对参数个数的合理性检查 */
    if (args < 0 || args > 3) {
	printf("Configuration error: invalid number of args (%d) for function %s\n", args, t->name);
	exit(1);
    }

    /* 分配参数测试值的范围，以保持测试总数不变，
       与参数个数无关 */
    if (args == 1) {
	arg_test_range[0] = TEST_RANGE;
    }
    else if (args == 2) {
	arg_test_range[0] = pow((double)TEST_RANGE, 0.5);  /* sqrt */
	arg_test_range[1] = arg_test_range[0];
    }
    else {
	arg_test_range[0] = pow((double)TEST_RANGE, 0.333); /* cbrt */
	arg_test_range[1] = arg_test_range[0];
	arg_test_range[2] = arg_test_range[0];
    }

    /* 对范围的合理性检查 */
    if (arg_test_range[0] < 1)
	arg_test_range[0] = 1;
    if (arg_test_range[1] < 1)
	arg_test_range[1] = 1;
    if (arg_test_range[2] < 1)
	arg_test_range[2] = 1;

    /* 为每个参数创建测试集 */
    for (i = 0; i < args; i++) {
	test_counts[i] =  gen_vals(arg_test_vals[i],
				   t->arg_ranges[i][0], /* min */
				   t->arg_ranges[i][1], /* max */
				   arg_test_range[i],
				   i);

    }

    /* 处理测试代码中的超时 */
    if (timeout_limit > 0) {
	int rc;
	rc = sigsetjmp(envbuf, 1);
	if (rc) {
	    /* 如果超时，控制将到达此处 */
	    errors = 1;
	    printf("ERROR: Test %s failed.\n  Timed out after %d secs (probably infinite loop)\n", t->name, timeout_limit);
	    return errors;
	}
	alarm(timeout_limit);
    }


    /* 测试函数没有参数的情况 */
    if (args == 0) {
	errors += test_0_arg(t->solution_funct, t->test_funct, t->name);
	return errors;
    }

    /*
     * 测试函数至少有一个参数的情况
     */

    /* 遍历第一个参数的值 */

    for (a1 = 0; a1 < test_counts[0]; a1++) {
	if (args == 1) {
	    errors += test_1_arg(t->solution_funct,
				 t->test_funct,
				 arg_test_vals[0][a1],
				 t->name);

	    /* 如果有错误则停止测试 */
	    if (errors)
		return errors;
	}
	else {
	    /* 如有必要，遍历第二个参数的值 */
	    for (a2 = 0; a2 < test_counts[1]; a2++) {
		if (args == 2) {
		    errors += test_2_arg(t->solution_funct,
					 t->test_funct,
					 arg_test_vals[0][a1],
					 arg_test_vals[1][a2],
					 t->name);

		    /* 如果有错误则停止测试 */
		    if (errors)
			return errors;
		}
		else {
		    /* 如有必要，遍历第三个参数的值 */
		    for (a3 = 0; a3 < test_counts[2]; a3++) {
			errors += test_3_arg(t->solution_funct,
					     t->test_funct,
					     arg_test_vals[0][a1],
					     arg_test_vals[1][a2],
					     arg_test_vals[2][a3],
					     t->name);

			/* 如果有错误则停止测试 */
			if (errors)
			    return errors;
		    } /* a3 */
		}
	    } /* a2 */
	}
    } /* a1 */


    return errors;
}

/*
 * run_tests - 运行一系列测试。返回错误数量
 */
static int run_tests()
{
    int i;
    int errors = 0;
    double points = 0.0;
    double max_points = 0.0;

    printf("Score\tRating\tErrors\tFunction\n");

    for (i = 0; test_set[i].solution_funct; i++) {
	int terrors;
	double tscore;
	double tpoints;
	if (!test_fname || strcmp(test_set[i].name,test_fname) == 0) {
	    int rating = global_rating ? global_rating : test_set[i].rating;
	    terrors = test_function(&test_set[i]);
	    errors += terrors;
	    tscore = terrors == 0 ? 1.0 : 0.0;
	    tpoints = rating * tscore;
	    points += tpoints;
	    max_points += rating;

	    if (grade || terrors < 1)
		printf(" %.0f\t%d\t%d\t%s\n",
		       tpoints, rating, terrors, test_set[i].name);

	}
    }

    printf("Total points: %.0f/%.0f\n", points, max_points);
    return errors;
}

/*
 * get_num_val - 从字符串中提取十六进制/十进制/或 float 值
 */
static int get_num_val(char *sval, unsigned *valp) {
    char *endp;

    /* 判断是整数还是 floating point */
    int ishex = 0;
    int isfloat = 0;
    int i;
    for (i = 0; sval[i]; i++) {
	switch (sval[i]) {
	case 'x':
	case 'X':
	    ishex = 1;
	    break;
	case 'e':
	case 'E':
	    if (!ishex)
		isfloat = 1;
	    break;
	case '.':
	    isfloat = 1;
	    break;
	default:
	    break;
	}
    }
    if (isfloat) {
	float fval = strtof(sval, &endp);
	if (!*endp) {
	    *valp = *(unsigned *) &fval;
	    return 1;
	}
	return 0;
    } else {
	long long int llval = strtoll(sval, &endp, 0);
	long long int upperbits = llval >> 31;
	/* 对于负数将给出 -1，对于正数将给出 0 或 1 */
	if (!*valp && (upperbits == 0 || upperbits == -1 || upperbits == 1)) {
	    *valp = (unsigned) llval;
	    return 1;
	}
	return 0;
    }
}


/*
 * usage - 显示用法信息
 */
static void usage(char *cmd) {
    printf("Usage: %s [-hg] [-r <n>] [-f <name> [-1|-2|-3 <val>]*] [-T <time limit>]\n", cmd);
    printf("  -1 <val>  指定第一个函数参数\n");
    printf("  -2 <val>  指定第二个函数参数\n");
    printf("  -3 <val>  指定第三个函数参数\n");
    printf("  -f <name> 仅测试指定名称的函数\n");
    printf("  -g        紧凑输出用于评分（不显示错误信息）\n");
    printf("  -h        打印此帮助信息\n");
    printf("  -r <n>    为所有题目给定统一的权重 n\n");
    printf("  -T <lim>  设置超时限制为 lim\n");
    exit(1);
}


/**************
 * 主例程
 **************/

int main(int argc, char *argv[])
{
    int errors;
    char c;

    /* 解析命令行参数 */
    while ((c = getopt(argc, argv, "hgf:r:T:1:2:3:")) != -1)
        switch (c) {
        case 'h': /* 帮助 */
	    usage(argv[0]);
	    break;
	case 'g': /* 自动评分器的评分选项 */
	    grade = 1;
	    break;
	case 'f': /* 仅测试一个函数 */
	    test_fname = strdup(optarg);
	    break;
	case 'r': /* 设置每个问题的全局评分权重 */
	    global_rating = atoi(optarg);
	    if (global_rating < 0)
		usage(argv[0]);
	    break;
	case '1': /* 获取第一个参数 */
	    has_arg[0] = get_num_val(optarg, &argval[0]);
	    if (!has_arg[0]) {
		printf("Bad argument '%s'\n", optarg);
		exit(0);
	    }
	    break;
	case '2': /* 获取第二个参数 */
	    has_arg[1] = get_num_val(optarg, &argval[1]);
	    if (!has_arg[1]) {
		printf("Bad argument '%s'\n", optarg);
		exit(0);
	    }
	    break;
	case '3': /* 获取第三个参数 */
	    has_arg[2] = get_num_val(optarg, &argval[2]);
	    if (!has_arg[2]) {
		printf("Bad argument '%s'\n", optarg);
		exit(0);
	    }
	    break;
	case 'T': /* 设置超时限制 */
	    timeout_limit = atoi(optarg);
	    break;
	default:
	    usage(argv[0]);
	}

    if (timeout_limit > 0) {
	Signal(SIGALRM, timeout_handler);
    }

    /* 测试每个函数 */
    errors = run_tests();

    return 0;
}

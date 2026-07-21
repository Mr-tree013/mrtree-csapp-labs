/*
 * mdriver.c - CS:APP Malloc Lab 驱动程序
 *
 * 使用一组 trace 文件来测试 mm.c 中的 malloc/free/realloc
 * 实现。
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <time.h>

#include "mm.h"
#include "memlib.h"
#include "fsecs.h"
#include "config.h"

/**********************
 * 常量和宏定义
 **********************/

/* 杂项 */
#define MAXLINE     1024 /* 最大字符串长度 */
#define HDRLINES       4 /* trace 文件中的头行数 */
#define LINENUM(i) (i+5) /* 将 trace 请求编号转换为行号（从 1 开始） */

/* 如果 p 对齐到 ALIGNMENT 字节，则返回 true */
#define IS_ALIGNED(p)  ((((unsigned int)(p)) % ALIGNMENT) == 0)

/******************************
 * 核心复合数据类型
 *****************************/

/* 记录每个 block 的 payload 范围 */
typedef struct range_t {
    char *lo;              /* payload 低地址 */
    char *hi;              /* payload 高地址 */
    struct range_t *next;  /* 链表中的下一个元素 */
} range_t;

/* 描述单个 trace 操作（分配器请求） */
typedef struct {
    enum {ALLOC, FREE, REALLOC} type; /* 请求类型 */
    int index;                        /* 供 free() 后续使用的索引 */
    int size;                         /* alloc/realloc 请求的字节大小 */
} traceop_t;

/* 保存一个 trace 文件的信息 */
typedef struct {
    int sugg_heapsize;   /* 建议的 heap 大小（未使用） */
    int num_ids;         /* alloc/realloc id 的数量 */
    int num_ops;         /* 不同请求的数量 */
    int weight;          /* 此 trace 的权重（未使用） */
    traceop_t *ops;      /* 请求数组 */
    char **blocks;       /* malloc/realloc 返回的指针数组... */
    size_t *block_sizes; /* ...以及相应的 payload 大小数组 */
} trace_t;

/*
 * 保存传给 xxx_speed 函数的参数，这些函数由 fcyc 计时。
 * 此结构体是必要的，因为 fcyc 只接受指针数组作为输入。
 */
typedef struct {
    trace_t *trace;
    range_t *ranges;
} speed_t;

/* 汇总某个 malloc 函数在某个 trace 上的重要统计信息 */
typedef struct {
    /* 同时适用于 libc malloc 和学生 malloc 包（mm.c） */
    double ops;      /* trace 中的操作次数（malloc/free/realloc） */
    int valid;       /* 分配器是否正确处理了此 trace */
    double secs;     /* 运行此 trace 所需的秒数 */

    /* 仅适用于学生 malloc 包 */
    double util;     /* 此 trace 的空间利用率（libc 始终为 0） */

    /* 注: secs 和 util 仅在 valid 为 true 时有定义 */
} stats_t;

/********************
 * 全局变量
 *******************/
int verbose = 0;        /* 控制详细输出的全局标志 */
static int errors = 0;  /* 运行学生 malloc 时发现的错误数量 */
char msg[MAXLINE];      /* 用于编写错误消息 */

/* 默认 trace 文件所在的目录 */
static char tracedir[MAXLINE] = TRACEDIR;

/* 默认 trace 文件的文件名列表 */
static char *default_tracefiles[] = {
    DEFAULT_TRACEFILES, NULL
};


/*********************
 * 函数原型声明
 *********************/

/* 以下函数操作 range 列表 */
static int add_range(range_t **ranges, char *lo, int size,
		     int tracenum, int opnum);
static void remove_range(range_t **ranges, char *lo);
static void clear_ranges(range_t **ranges);

/* 以下函数读取、分配和释放 trace 的存储空间 */
static trace_t *read_trace(char *tracedir, char *filename);
static void free_trace(trace_t *trace);

/* 评估 libc malloc 正确性和速度的例程 */
static int eval_libc_valid(trace_t *trace, int tracenum);
static void eval_libc_speed(void *ptr);

/* 评估 mm.c 中学生 malloc 包的正确性、空间利用率和速度的例程 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges);
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges);
static void eval_mm_speed(void *ptr);

/* 各种辅助例程 */
static void printresults(int n, stats_t *stats);
static void usage(void);
static void unix_error(char *msg);
static void malloc_error(int tracenum, int opnum, char *msg);
static void app_error(char *msg);

/**************
 * 主程序
 **************/
int main(int argc, char **argv)
{
    int i;
    char c;
    char **tracefiles = NULL;  /* 以 null 结尾的 trace 文件名数组 */
    int num_tracefiles = 0;    /* 该数组中 trace 的数量 */
    trace_t *trace = NULL;     /* 在内存中存储单个 trace 文件 */
    range_t *ranges = NULL;    /* 跟踪一个 trace 的 block 范围 */
    stats_t *libc_stats = NULL;/* 每个 trace 的 libc 统计信息 */
    stats_t *mm_stats = NULL;  /* 每个 trace 的 mm（即学生）统计信息 */
    speed_t speed_params;      /* xx_speed 例程的输入参数 */

    int team_check = 1;  /* 如果设置，检查团队结构体（由 -a 重置） */
    int run_libc = 0;    /* 如果设置，运行 libc malloc（由 -l 设置） */
    int autograder = 0;  /* 如果设置，为自动评分器输出摘要信息（-g） */

    /* 用于计算性能指数的临时变量 */
    double secs, ops, util, avg_mm_util, avg_mm_throughput, p1, p2, perfindex;
    int numcorrect;

    /*
     * 读取并解析命令行参数
     */
    while ((c = getopt(argc, argv, "f:t:hvVgal")) != EOF) {
        switch (c) {
	case 'g': /* 为自动评分器生成摘要信息 */
	    autograder = 1;
	    break;
        case 'f': /* 只使用一个特定的 trace 文件（相对于当前目录） */
            num_tracefiles = 1;
            if ((tracefiles = realloc(tracefiles, 2*sizeof(char *))) == NULL)
		unix_error("ERROR: realloc failed in main");
	    strcpy(tracedir, "./");
            tracefiles[0] = strdup(optarg);
            tracefiles[1] = NULL;
            break;
	case 't': /* trace 文件所在的目录 */
	    if (num_tracefiles == 1) /* 如果已遇到 -f，则忽略 */
		break;
	    strcpy(tracedir, optarg);
	    if (tracedir[strlen(tracedir)-1] != '/')
		strcat(tracedir, "/"); /* 路径始终以 "/" 结尾 */
	    break;
        case 'a': /* 不检查团队结构体 */
            team_check = 0;
            break;
        case 'l': /* 运行 libc malloc */
            run_libc = 1;
            break;
        case 'v': /* 打印每个 trace 的性能详情 */
            verbose = 1;
            break;
        case 'V': /* 输出比 -v 更详细的信息 */
            verbose = 2;
            break;
        case 'h': /* 打印帮助信息 */
	    usage();
            exit(0);
        default:
	    usage();
            exit(1);
        }
    }

    /*
     * 检查并打印团队信息
     */
    if (team_check) {
	/* 学生必须填写他们的团队信息 */
	if (!strcmp(team.teamname, "")) {
	    printf("ERROR: Please provide the information about your team in mm.c.\n");
	    exit(1);
	} else
	    printf("Team Name:%s\n", team.teamname);
	if ((*team.name1 == '\0') || (*team.id1 == '\0')) {
	    printf("ERROR.  You must fill in all team member 1 fields!\n");
	    exit(1);
	}
	else
	    printf("Member 1 :%s:%s\n", team.name1, team.id1);

	if (((*team.name2 != '\0') && (*team.id2 == '\0')) ||
	    ((*team.name2 == '\0') && (*team.id2 != '\0'))) {
	    printf("ERROR.  You must fill in all or none of the team member 2 ID fields!\n");
	    exit(1);
	}
	else if (*team.name2 != '\0')
	    printf("Member 2 :%s:%s\n", team.name2, team.id2);
    }

    /*
     * 如果没有 -f 命令行参数，则使用 default_traces[] 中
     * 定义的整组 trace 文件
     */
    if (tracefiles == NULL) {
        tracefiles = default_tracefiles;
        num_tracefiles = sizeof(default_tracefiles) / sizeof(char *) - 1;
	printf("Using default tracefiles in %s\n", tracedir);
    }

    /* 初始化计时包 */
    init_fsecs();

    /*
     * 可选地运行并评估 libc malloc 包
     */
    if (run_libc) {
	if (verbose > 1)
	    printf("\nTesting libc malloc\n");

	/* 分配 libc 统计数组，每个 trace 文件一个 stats_t 结构体 */
	libc_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
	if (libc_stats == NULL)
	    unix_error("libc_stats calloc in main failed");

	/* 使用 K-best 方案评估 libc malloc 包 */
	for (i=0; i < num_tracefiles; i++) {
	    trace = read_trace(tracedir, tracefiles[i]);
	    libc_stats[i].ops = trace->num_ops;
	    if (verbose > 1)
		printf("Checking libc malloc for correctness, ");
	    libc_stats[i].valid = eval_libc_valid(trace, i);
	    if (libc_stats[i].valid) {
		speed_params.trace = trace;
		if (verbose > 1)
		    printf("and performance.\n");
		libc_stats[i].secs = fsecs(eval_libc_speed, &speed_params);
	    }
	    free_trace(trace);
	}

	/* 以紧凑表格形式显示 libc 结果 */
	if (verbose) {
	    printf("\nResults for libc malloc:\n");
	    printresults(num_tracefiles, libc_stats);
	}
    }

    /*
     * 始终运行并评估学生的 mm 包
     */
    if (verbose > 1)
	printf("\nTesting mm malloc\n");

    /* 分配 mm 统计数组，每个 trace 文件一个 stats_t 结构体 */
    mm_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
    if (mm_stats == NULL)
	unix_error("mm_stats calloc in main failed");

    /* 初始化 memlib.c 中的模拟内存系统 */
    mem_init();

    /* 使用 K-best 方案评估学生的 mm malloc 包 */
    for (i=0; i < num_tracefiles; i++) {
	trace = read_trace(tracedir, tracefiles[i]);
	mm_stats[i].ops = trace->num_ops;
	if (verbose > 1)
	    printf("Checking mm_malloc for correctness, ");
	mm_stats[i].valid = eval_mm_valid(trace, i, &ranges);
	if (mm_stats[i].valid) {
	    if (verbose > 1)
		printf("efficiency, ");
	    mm_stats[i].util = eval_mm_util(trace, i, &ranges);
	    speed_params.trace = trace;
	    speed_params.ranges = ranges;
	    if (verbose > 1)
		printf("and performance.\n");
	    mm_stats[i].secs = fsecs(eval_mm_speed, &speed_params);
	}
	free_trace(trace);
    }

    /* 以紧凑表格形式显示 mm 结果 */
    if (verbose) {
	printf("\nResults for mm malloc:\n");
	printresults(num_tracefiles, mm_stats);
	printf("\n");
    }

    /*
     * 汇总学生 mm 包的统计信息
     */
    secs = 0;
    ops = 0;
    util = 0;
    numcorrect = 0;
    for (i=0; i < num_tracefiles; i++) {
	secs += mm_stats[i].secs;
	ops += mm_stats[i].ops;
	util += mm_stats[i].util;
	if (mm_stats[i].valid)
	    numcorrect++;
    }
    avg_mm_util = util/num_tracefiles;

    /*
     * 计算并打印性能指数
     */
    if (errors == 0) {
	avg_mm_throughput = ops/secs;

	p1 = UTIL_WEIGHT * avg_mm_util;
	if (avg_mm_throughput > AVG_LIBC_THRUPUT) {
	    p2 = (double)(1.0 - UTIL_WEIGHT);
	}
	else {
	    p2 = ((double) (1.0 - UTIL_WEIGHT)) *
		(avg_mm_throughput/AVG_LIBC_THRUPUT);
	}

	perfindex = (p1 + p2)*100.0;
	printf("Perf index = %.0f (util) + %.0f (thru) = %.0f/100\n",
	       p1*100,
	       p2*100,
	       perfindex);

    }
    else { /* 存在错误 */
	perfindex = 0.0;
	printf("Terminated with %d errors\n", errors);
    }

    if (autograder) {
	printf("correct:%d\n", numcorrect);
	printf("perfidx:%.0f\n", perfindex);
    }

    exit(0);
}


/*****************************************************************
 * 以下例程操作 range 列表，该列表跟踪每个已分配 block
 * 的 payload 范围。我们使用 range 列表来检测任何
 * 重叠的已分配 block。
 ****************************************************************/

/*
 * add_range - 根据 trace tracenum 中编号为 opnum 的请求，
 *     我们刚刚调用了学生的 mm_malloc，在地址 lo 处分配了
 *     大小为 size 字节的 block。在检查该 block 的正确性之后，
 *     我们为此 block 创建一个 range 结构体并将其添加到 range 列表中。
 */
static int add_range(range_t **ranges, char *lo, int size,
		     int tracenum, int opnum)
{
    char *hi = lo + size - 1;
    range_t *p;
    char msg[MAXLINE];

    assert(size > 0);

    /* payload 地址必须对齐到 ALIGNMENT 字节 */
    if (!IS_ALIGNED(lo)) {
	sprintf(msg, "Payload address (%p) not aligned to %d bytes",
		lo, ALIGNMENT);
        malloc_error(tracenum, opnum, msg);
        return 0;
    }

    /* payload 必须位于 heap 范围内 */
    if ((lo < (char *)mem_heap_lo()) || (lo > (char *)mem_heap_hi()) ||
	(hi < (char *)mem_heap_lo()) || (hi > (char *)mem_heap_hi())) {
	sprintf(msg, "Payload (%p:%p) lies outside heap (%p:%p)",
		lo, hi, mem_heap_lo(), mem_heap_hi());
	malloc_error(tracenum, opnum, msg);
        return 0;
    }

    /* payload 不得与任何其他 payload 重叠 */
    for (p = *ranges;  p != NULL;  p = p->next) {
        if ((lo >= p->lo && lo <= p-> hi) ||
            (hi >= p->lo && hi <= p->hi)) {
	    sprintf(msg, "Payload (%p:%p) overlaps another payload (%p:%p)\n",
		    lo, hi, p->lo, p->hi);
	    malloc_error(tracenum, opnum, msg);
	    return 0;
        }
    }

    /*
     * 一切看起来没问题，因此记录此 block 的范围：
     * 创建一个 range 结构体并将其添加到 range 列表中。
     */
    if ((p = (range_t *)malloc(sizeof(range_t))) == NULL)
	unix_error("malloc error in add_range");
    p->next = *ranges;
    p->lo = lo;
    p->hi = hi;
    *ranges = p;
    return 1;
}

/*
 * remove_range - 释放 payload 起始地址为 lo 的 block 的 range 记录
 */
static void remove_range(range_t **ranges, char *lo)
{
    range_t *p;
    range_t **prevpp = ranges;
    int size;

    for (p = *ranges;  p != NULL; p = p->next) {
        if (p->lo == lo) {
	    *prevpp = p->next;
            size = p->hi - p->lo + 1;
            free(p);
            break;
        }
        prevpp = &(p->next);
    }
}

/*
 * clear_ranges - 释放一个 trace 的所有 range 记录
 */
static void clear_ranges(range_t **ranges)
{
    range_t *p;
    range_t *pnext;

    for (p = *ranges;  p != NULL;  p = pnext) {
        pnext = p->next;
        free(p);
    }
    *ranges = NULL;
}


/**********************************************
 * 以下例程操作 trace 文件
 *********************************************/

/*
 * read_trace - 读取一个 trace 文件并将其存储在内存中
 */
static trace_t *read_trace(char *tracedir, char *filename)
{
    FILE *tracefile;
    trace_t *trace;
    char type[MAXLINE];
    char path[MAXLINE];
    unsigned index, size;
    unsigned max_index = 0;
    unsigned op_index;

    if (verbose > 1)
	printf("Reading tracefile: %s\n", filename);

    /* 分配 trace 记录 */
    if ((trace = (trace_t *) malloc(sizeof(trace_t))) == NULL)
	unix_error("malloc 1 failed in read_trance");

    /* 读取 trace 文件头 */
    strcpy(path, tracedir);
    strcat(path, filename);
    if ((tracefile = fopen(path, "r")) == NULL) {
	sprintf(msg, "Could not open %s in read_trace", path);
	unix_error(msg);
    }
    fscanf(tracefile, "%d", &(trace->sugg_heapsize)); /* 未使用 */
    fscanf(tracefile, "%d", &(trace->num_ids));
    fscanf(tracefile, "%d", &(trace->num_ops));
    fscanf(tracefile, "%d", &(trace->weight));        /* 未使用 */

    /* 我们将 trace 中的每个请求行存储在此数组中 */
    if ((trace->ops =
	 (traceop_t *)malloc(trace->num_ops * sizeof(traceop_t))) == NULL)
	unix_error("malloc 2 failed in read_trace");

    /* 我们将在此处保存一个指向已分配 block 的指针数组... */
    if ((trace->blocks =
	 (char **)malloc(trace->num_ids * sizeof(char *))) == NULL)
	unix_error("malloc 3 failed in read_trace");

    /* ...以及每个 block 对应的字节大小 */
    if ((trace->block_sizes =
	 (size_t *)malloc(trace->num_ids * sizeof(size_t))) == NULL)
	unix_error("malloc 4 failed in read_trace");

    /* 读取 trace 文件中的每一行请求 */
    index = 0;
    op_index = 0;
    while (fscanf(tracefile, "%s", type) != EOF) {
	switch(type[0]) {
	case 'a':
	    fscanf(tracefile, "%u %u", &index, &size);
	    trace->ops[op_index].type = ALLOC;
	    trace->ops[op_index].index = index;
	    trace->ops[op_index].size = size;
	    max_index = (index > max_index) ? index : max_index;
	    break;
	case 'r':
	    fscanf(tracefile, "%u %u", &index, &size);
	    trace->ops[op_index].type = REALLOC;
	    trace->ops[op_index].index = index;
	    trace->ops[op_index].size = size;
	    max_index = (index > max_index) ? index : max_index;
	    break;
	case 'f':
	    fscanf(tracefile, "%ud", &index);
	    trace->ops[op_index].type = FREE;
	    trace->ops[op_index].index = index;
	    break;
	default:
	    printf("Bogus type character (%c) in tracefile %s\n",
		   type[0], path);
	    exit(1);
	}
	op_index++;

    }
    fclose(tracefile);
    assert(max_index == trace->num_ids - 1);
    assert(trace->num_ops == op_index);

    return trace;
}

/*
 * free_trace - 释放 trace 记录及其指向的三个数组，
 *              这些数组全部在 read_trace() 中分配。
 */
void free_trace(trace_t *trace)
{
    free(trace->ops);         /* 释放三个数组... */
    free(trace->blocks);
    free(trace->block_sizes);
    free(trace);              /* 以及 trace 记录本身... */
}

/**********************************************************************
 * 以下函数评估 libc 和 mm malloc 包的正确性、空间利用率
 * 和 throughput。
 **********************************************************************/

/*
 * eval_mm_valid - 检查 mm malloc 包的正确性
 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges)
{
    int i, j;
    int index;
    int size;
    int oldsize;
    char *newp;
    char *oldp;
    char *p;

    /* 重置 heap 并释放 range 列表中的所有记录 */
    mem_reset_brk();
    clear_ranges(ranges);

    /* 调用 mm 包的 init 函数 */
    if (mm_init() < 0) {
	malloc_error(tracenum, 0, "mm_init failed.");
	return 0;
    }

    /* 按顺序解释 trace 中的每个操作 */
    for (i = 0;  i < trace->num_ops;  i++) {
	index = trace->ops[i].index;
	size = trace->ops[i].size;

        switch (trace->ops[i].type) {

        case ALLOC: /* mm_malloc */

	    /* 调用学生的 malloc */
	    if ((p = mm_malloc(size)) == NULL) {
		malloc_error(tracenum, i, "mm_malloc failed.");
		return 0;
	    }

	    /*
	     * 测试新 block 的范围的正确性，如果通过则
	     * 将其添加到 range 列表中。该 block 必须正确对齐，
	     * 且不得与任何当前已分配的 block 重叠。
	     */
	    if (add_range(ranges, p, size, tracenum, i) == 0)
		return 0;

	    /* 添加者: cgw
	     * 用索引的低字节填充范围。这将在后续重新分配该 block
	     * 并希望确保旧数据已复制到新 block 时使用。
	     */
	    memset(p, index & 0xFF, size);

	    /* 记住此区域 */
	    trace->blocks[index] = p;
	    trace->block_sizes[index] = size;
	    break;

        case REALLOC: /* mm_realloc */

	    /* 调用学生的 realloc */
	    oldp = trace->blocks[index];
	    if ((newp = mm_realloc(oldp, size)) == NULL) {
		malloc_error(tracenum, i, "mm_realloc failed.");
		return 0;
	    }

	    /* 从 range 列表中删除旧区域 */
	    remove_range(ranges, oldp);

	    /* 检查新 block 的正确性并将其添加到 range 列表中 */
	    if (add_range(ranges, newp, size, tracenum, i) == 0)
		return 0;

	    /* 添加者: cgw
	     * 确保新 block 包含来自旧 block 的数据，
	     * 然后用新索引的低字节填充新 block。
	     */
	    oldsize = trace->block_sizes[index];
	    if (size < oldsize) oldsize = size;
	    for (j = 0; j < oldsize; j++) {
	      if (newp[j] != (index & 0xFF)) {
		malloc_error(tracenum, i, "mm_realloc did not preserve the "
			     "data from old block");
		return 0;
	      }
	    }
	    memset(newp, index & 0xFF, size);

	    /* 记住此区域 */
	    trace->blocks[index] = newp;
	    trace->block_sizes[index] = size;
	    break;

        case FREE: /* mm_free */

	    /* 从列表中删除区域并调用学生的 free 函数 */
	    p = trace->blocks[index];
	    remove_range(ranges, p);
	    mm_free(p);
	    break;

	default:
	    app_error("Nonexistent request type in eval_mm_valid");
        }

    }

    /* 据我们所知，这是一个有效的 malloc 包 */
    return 1;
}

/*
 * eval_mm_util - 评估学生包的空间利用率
 *   基本思路是：记住一个最优分配器的 heap 高水位线 "hwm"，
 *   即没有间隙和内部碎片的理想状态。利用率是 hwm/heapsize 的
 *   比值，其中 heapsize 是在 trace 上运行学生 malloc 包之后
 *   heap 的字节大小。注意，我们的 mem_sbrk() 实现不允许学生
 *   递减 brk 指针，因此 brk 始终是 heap 的高水位线。
 *
 */
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges)
{
    int i;
    int index;
    int size, newsize, oldsize;
    int max_total_size = 0;
    int total_size = 0;
    char *p;
    char *newp, *oldp;

    /* 初始化 heap 和 mm malloc 包 */
    mem_reset_brk();
    if (mm_init() < 0)
	app_error("mm_init failed in eval_mm_util");

    for (i = 0;  i < trace->num_ops;  i++) {
        switch (trace->ops[i].type) {

        case ALLOC: /* mm_alloc */
	    index = trace->ops[i].index;
	    size = trace->ops[i].size;

	    if ((p = mm_malloc(size)) == NULL)
		app_error("mm_malloc failed in eval_mm_util");

	    /* 记住区域和大小 */
	    trace->blocks[index] = p;
	    trace->block_sizes[index] = size;

	    /* 跟踪所有已分配 block 的当前总大小 */
	    total_size += size;

	    /* 更新统计信息 */
	    max_total_size = (total_size > max_total_size) ?
		total_size : max_total_size;
	    break;

	case REALLOC: /* mm_realloc */
	    index = trace->ops[i].index;
	    newsize = trace->ops[i].size;
	    oldsize = trace->block_sizes[index];

	    oldp = trace->blocks[index];
	    if ((newp = mm_realloc(oldp,newsize)) == NULL)
		app_error("mm_realloc failed in eval_mm_util");

	    /* 记住区域和大小 */
	    trace->blocks[index] = newp;
	    trace->block_sizes[index] = newsize;

	    /* 跟踪所有已分配 block 的当前总大小 */
	    total_size += (newsize - oldsize);

	    /* 更新统计信息 */
	    max_total_size = (total_size > max_total_size) ?
		total_size : max_total_size;
	    break;

        case FREE: /* mm_free */
	    index = trace->ops[i].index;
	    size = trace->block_sizes[index];
	    p = trace->blocks[index];

	    mm_free(p);

	    /* 跟踪所有已分配 block 的当前总大小 */
	    total_size -= size;

	    break;

	default:
	    app_error("Nonexistent request type in eval_mm_util");

        }
    }

    return ((double)max_total_size / (double)mem_heapsize());
}


/*
 * eval_mm_speed - 这是 fcyc() 用来测量 mm malloc 包
 *    运行时间的函数。
 */
static void eval_mm_speed(void *ptr)
{
    int i, index, size, newsize;
    char *p, *newp, *oldp, *block;
    trace_t *trace = ((speed_t *)ptr)->trace;

    /* 重置 heap 并初始化 mm 包 */
    mem_reset_brk();
    if (mm_init() < 0)
	app_error("mm_init failed in eval_mm_speed");

    /* 解释每个 trace 请求 */
    for (i = 0;  i < trace->num_ops;  i++)
        switch (trace->ops[i].type) {

        case ALLOC: /* mm_malloc */
            index = trace->ops[i].index;
            size = trace->ops[i].size;
            if ((p = mm_malloc(size)) == NULL)
		app_error("mm_malloc error in eval_mm_speed");
            trace->blocks[index] = p;
            break;

	case REALLOC: /* mm_realloc */
	    index = trace->ops[i].index;
            newsize = trace->ops[i].size;
	    oldp = trace->blocks[index];
            if ((newp = mm_realloc(oldp,newsize)) == NULL)
		app_error("mm_realloc error in eval_mm_speed");
            trace->blocks[index] = newp;
            break;

        case FREE: /* mm_free */
            index = trace->ops[i].index;
            block = trace->blocks[index];
            mm_free(block);
            break;

	default:
	    app_error("Nonexistent request type in eval_mm_valid");
        }
}

/*
 * eval_libc_valid - 我们运行此函数以确保 libc malloc
 *    可以在整组 trace 上成功运行完毕。
 *    我们采取保守策略：如果任何 libc malloc 调用失败，则终止。
 *
 */
static int eval_libc_valid(trace_t *trace, int tracenum)
{
    int i, newsize;
    char *p, *newp, *oldp;

    for (i = 0;  i < trace->num_ops;  i++) {
        switch (trace->ops[i].type) {

        case ALLOC: /* malloc */
	    if ((p = malloc(trace->ops[i].size)) == NULL) {
		malloc_error(tracenum, i, "libc malloc failed");
		unix_error("System message");
	    }
	    trace->blocks[trace->ops[i].index] = p;
	    break;

	case REALLOC: /* realloc */
            newsize = trace->ops[i].size;
	    oldp = trace->blocks[trace->ops[i].index];
	    if ((newp = realloc(oldp, newsize)) == NULL) {
		malloc_error(tracenum, i, "libc realloc failed");
		unix_error("System message");
	    }
	    trace->blocks[trace->ops[i].index] = newp;
	    break;

        case FREE: /* free */
	    free(trace->blocks[trace->ops[i].index]);
	    break;

	default:
	    app_error("invalid operation type  in eval_libc_valid");
	}
    }

    return 1;
}

/*
 * eval_libc_speed - 这是 fcyc() 用来测量 libc malloc 包
 *    在整组 trace 上运行时间的函数。
 */
static void eval_libc_speed(void *ptr)
{
    int i;
    int index, size, newsize;
    char *p, *newp, *oldp, *block;
    trace_t *trace = ((speed_t *)ptr)->trace;

    for (i = 0;  i < trace->num_ops;  i++) {
        switch (trace->ops[i].type) {
        case ALLOC: /* malloc */
	    index = trace->ops[i].index;
	    size = trace->ops[i].size;
	    if ((p = malloc(size)) == NULL)
		unix_error("malloc failed in eval_libc_speed");
	    trace->blocks[index] = p;
	    break;

	case REALLOC: /* realloc */
	    index = trace->ops[i].index;
	    newsize = trace->ops[i].size;
	    oldp = trace->blocks[index];
	    if ((newp = realloc(oldp, newsize)) == NULL)
		unix_error("realloc failed in eval_libc_speed\n");

	    trace->blocks[index] = newp;
	    break;

        case FREE: /* free */
	    index = trace->ops[i].index;
	    block = trace->blocks[index];
	    free(block);
	    break;
	}
    }
}

/*************************************
 * 一些杂项辅助例程
 ************************************/


/*
 * printresults - 打印某个 malloc 包的性能摘要
 */
static void printresults(int n, stats_t *stats)
{
    int i;
    double secs = 0;
    double ops = 0;
    double util = 0;

    /* 打印每个 trace 的单独结果 */
    printf("%5s%7s %5s%8s%10s%6s\n",
	   "trace", " valid", "util", "ops", "secs", "Kops");
    for (i=0; i < n; i++) {
	if (stats[i].valid) {
	    printf("%2d%10s%5.0f%%%8.0f%10.6f%6.0f\n",
		   i,
		   "yes",
		   stats[i].util*100.0,
		   stats[i].ops,
		   stats[i].secs,
		   (stats[i].ops/1e3)/stats[i].secs);
	    secs += stats[i].secs;
	    ops += stats[i].ops;
	    util += stats[i].util;
	}
	else {
	    printf("%2d%10s%6s%8s%10s%6s\n",
		   i,
		   "no",
		   "-",
		   "-",
		   "-",
		   "-");
	}
    }

    /* 打印整组 trace 的汇总结果 */
    if (errors == 0) {
	printf("%12s%5.0f%%%8.0f%10.6f%6.0f\n",
	       "Total       ",
	       (util/n)*100.0,
	       ops,
	       secs,
	       (ops/1e3)/secs);
    }
    else {
	printf("%12s%6s%8s%10s%6s\n",
	       "Total       ",
	       "-",
	       "-",
	       "-",
	       "-");
    }

}

/*
 * app_error - 报告任意应用程序错误
 */
void app_error(char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

/*
 * unix_error - 报告 Unix 风格的错误
 */
void unix_error(char *msg)
{
    printf("%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * malloc_error - 报告 mm_malloc 包返回的错误
 */
void malloc_error(int tracenum, int opnum, char *msg)
{
    errors++;
    printf("ERROR [trace %d, line %d]: %s\n", tracenum, LINENUM(opnum), msg);
}

/*
 * usage - 解释命令行参数
 */
static void usage(void)
{
    fprintf(stderr, "Usage: mdriver [-hvVal] [-f <file>] [-t <dir>]\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, "\t-a         不检查团队结构体。\n");
    fprintf(stderr, "\t-f <file>  使用 <file> 作为 trace 文件。\n");
    fprintf(stderr, "\t-g         为自动评分器生成摘要信息。\n");
    fprintf(stderr, "\t-h         打印此帮助消息。\n");
    fprintf(stderr, "\t-l         同时运行 libc malloc。\n");
    fprintf(stderr, "\t-t <dir>   查找默认 trace 文件的目录。\n");
    fprintf(stderr, "\t-v         打印每个 trace 的性能详情。\n");
    fprintf(stderr, "\t-V         打印额外的调试信息。\n");
}

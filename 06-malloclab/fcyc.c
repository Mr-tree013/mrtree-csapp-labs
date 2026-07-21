/*
 * fcyc.c - 估算函数 f 所消耗的时间（以 CPU 周期计）
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 *
 * 使用 clock.c 中的周期计时器例程来估算
 * 函数 f 的 CPU 周期时间。
 */
#include <stdlib.h>
#include <sys/times.h>
#include <stdio.h>

#include "fcyc.h"
#include "clock.h"

/* 默认值 */
#define K 3                  /* K-best 方案中的 K 值 */
#define MAXSAMPLES 20        /* 在 MAXSAMPLES 之后放弃 */
#define EPSILON 0.01         /* K 个样本之间的误差应在 EPSILON 以内 */
#define COMPENSATE 0         /* 1 -> 尝试补偿时钟 tick */
#define CLEAR_CACHE 0        /* 运行测试函数前清除缓存 */
#define CACHE_BYTES (1<<19)  /* 最大缓存大小（字节） */
#define CACHE_BLOCK 32       /* 缓存块大小（字节） */

static int kbest = K;
static int maxsamples = MAXSAMPLES;
static double epsilon = EPSILON;
static int compensate = COMPENSATE;
static int clear_cache = CLEAR_CACHE;
static int cache_bytes = CACHE_BYTES;
static int cache_block = CACHE_BLOCK;

static int *cache_buf = NULL;

static double *values = NULL;
static int samplecount = 0;

/* 仅用于调试 */
#define KEEP_VALS 0
#define KEEP_SAMPLES 0

#if KEEP_SAMPLES
static double *samples = NULL;
#endif

/*
 * init_sampler - 开始新的采样过程
 */
static void init_sampler()
{
    if (values)
	free(values);
    values = calloc(kbest, sizeof(double));
#if KEEP_SAMPLES
    if (samples)
	free(samples);
    /* 额外分配用于环形分析 */
    samples = calloc(maxsamples+kbest, sizeof(double));
#endif
    samplecount = 0;
}

/*
 * add_sample - 添加新样本
 */
static void add_sample(double val)
{
    int pos = 0;
    if (samplecount < kbest) {
	pos = samplecount;
	values[pos] = val;
    } else if (val < values[kbest-1]) {
	pos = kbest-1;
	values[pos] = val;
    }
#if KEEP_SAMPLES
    samples[samplecount] = val;
#endif
    samplecount++;
    /* 插入排序 */
    while (pos > 0 && values[pos-1] > values[pos]) {
	double temp = values[pos-1];
	values[pos-1] = values[pos];
	values[pos] = temp;
	pos--;
    }
}

/*
 * has_converged - kbest 个最小测量值是否在 epsilon 范围内收敛？
 */
static int has_converged()
{
    return
	(samplecount >= kbest) &&
	((1 + epsilon)*values[0] >= values[kbest-1]);
}

/*
 * clear - 清除缓存的代码
 */
static volatile int sink = 0;

static void clear()
{
    int x = sink;
    int *cptr, *cend;
    int incr = cache_block/sizeof(int);
    if (!cache_buf) {
	cache_buf = malloc(cache_bytes);
	if (!cache_buf) {
	    fprintf(stderr, "Fatal error.  Malloc returned null when trying to clear cache\n");
	    exit(1);
	}
    }
    cptr = (int *) cache_buf;
    cend = cptr + cache_bytes/sizeof(int);
    while (cptr < cend) {
	x += *cptr;
	cptr += incr;
    }
    sink = x;
}

/*
 * fcyc - 使用 K-best 方案估算函数 f 的运行时间
 */
double fcyc(test_funct f, void *argp)
{
    double result;
    init_sampler();
    if (compensate) {
	do {
	    double cyc;
	    if (clear_cache)
		clear();
	    start_comp_counter();
	    f(argp);
	    cyc = get_comp_counter();
	    add_sample(cyc);
	} while (!has_converged() && samplecount < maxsamples);
    } else {
	do {
	    double cyc;
	    if (clear_cache)
		clear();
	    start_counter();
	    f(argp);
	    cyc = get_counter();
	    add_sample(cyc);
	} while (!has_converged() && samplecount < maxsamples);
    }
#ifdef DEBUG
    {
	int i;
	printf(" %d smallest values: [", kbest);
	for (i = 0; i < kbest; i++)
	    printf("%.0f%s", values[i], i==kbest-1 ? "]\n" : ", ");
    }
#endif
    result = values[0];
#if !KEEP_VALS
    free(values);
    values = NULL;
#endif
    return result;
}


/*************************************************************
 * 设置测量例程使用的各种参数
 ************************************************************/

/*
 * set_fcyc_clear_cache - 设置后，在每次测量前运行
 *     清除缓存的代码。
 *     默认值 = 0
 */
void set_fcyc_clear_cache(int clear)
{
    clear_cache = clear;
}

/*
 * set_fcyc_cache_size - 设置清除缓存时要使用的缓存大小
 *     默认值 = 1<<19 (512KB)
 */
void set_fcyc_cache_size(int bytes)
{
    if (bytes != cache_bytes) {
	cache_bytes = bytes;
	if (cache_buf) {
	    free(cache_buf);
	    cache_buf = NULL;
	}
    }
}

/*
 * set_fcyc_cache_block - 设置缓存块大小
 *     默认值 = 32
 */
void set_fcyc_cache_block(int bytes) {
    cache_block = bytes;
}


/*
 * set_fcyc_compensate - 设置后，将尝试补偿
 *     定时器中断开销
 *     默认值 = 0
 */
void set_fcyc_compensate(int compensate_arg)
{
    compensate = compensate_arg;
}

/*
 * set_fcyc_k - K-best 测量方案中的 K 值
 *     默认值 = 3
 */
void set_fcyc_k(int k)
{
    kbest = k;
}

/*
 * set_fcyc_maxsamples - 在某个容差范围内寻找 K-best 时
 *     的最大采样次数。超过后，直接返回找到的最佳样本。
 *     默认值 = 20
 */
void set_fcyc_maxsamples(int maxsamples_arg)
{
    maxsamples = maxsamples_arg;
}

/*
 * set_fcyc_epsilon - K-best 所需的容差
 *     默认值 = 0.01
 */
void set_fcyc_epsilon(double epsilon_arg)
{
    epsilon = epsilon_arg;
}

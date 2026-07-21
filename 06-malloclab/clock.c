/*
 * clock.c - 用于访问 x86、Alpha 和 Sparc 机器上
 *           周期计数器的例程。
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/times.h>
#include "clock.h"


/*******************************************************
 * 机器相关函数
 *
 * 注: 常量 __i386__ 和 __alpha
 * 由 GCC 在调用 C 预处理器时设置。
 * 你可以通过 gcc -v 自行验证这一点。
 *******************************************************/

#if defined(__i386__)
/*******************************************************
 * Pentium 版本的 start_counter() 和 get_counter()
 *******************************************************/


/* $begin x86cyclecounter */
/* 初始化周期计数器 */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;


/* 将 *hi 和 *lo 设置为周期计数器的高位和低位部分。
   实现需要使用汇编代码通过 rdtsc 指令实现。 */
void access_counter(unsigned *hi, unsigned *lo)
{
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1"   /* 读取周期计数器 */
	: "=r" (*hi), "=r" (*lo)                /* 并将结果移入 */
	: /* 没有输入 */                          /* 两个输出 */
	: "%edx", "%eax");
}

/* 记录周期计数器的当前值。 */
void start_counter()
{
    access_counter(&cyc_hi, &cyc_lo);
}

/* 返回自上次调用 start_counter 以来的周期数。 */
double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;

    /* 获取周期计数器 */
    access_counter(&ncyc_hi, &ncyc_lo);

    /* 执行双精度减法 */
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double) hi * (1 << 30) * 4 + lo;
    if (result < 0) {
	fprintf(stderr, "Error: counter returns neg value: %.0f\n", result);
    }
    return result;
}
/* $end x86cyclecounter */

#elif defined(__alpha)

/****************************************************
 * Alpha 版本的 start_counter() 和 get_counter()
 ***************************************************/

/* 初始化周期计数器 */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;


/* 使用 Alpha 周期定时器来计算周期。然后
   使用测量的时钟速度来计算秒数。
*/

/*
 * counterRoutine 是一个 Alpha 指令数组，用于访问
 * Alpha 处理器的周期计数器。它使用 rpcc
 * 指令来访问计数器。这个 64 位寄存器被
 * 分为两部分。低 32 位是当前进程使用的
 * 周期数。高 32 位是墙上时钟周期。
 * 这些指令读取计数器，并将低 32 位转换为
 * 无符号整数——这就是用户态计数器值。
 * 注意: 该计数器的时间跨度非常有限。对于
 * 450MHz 的时钟，该计数器可以计时大约 9 秒。 */
static unsigned int counterRoutine[] =
{
    0x601fc000u,
    0x401f0000u,
    0x6bfa8001u
};

/* 将上述指令转换为一个函数。 */
static unsigned int (*counter)(void)= (void *)counterRoutine;


void start_counter()
{
    /* 获取周期计数器 */
    cyc_hi = 0;
    cyc_lo = counter();
}

double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;
    ncyc_lo = counter();
    ncyc_hi = 0;
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double) hi * (1 << 30) * 4 + lo;
    if (result < 0) {
	fprintf(stderr, "Error: Cycle counter returning negative value: %.0f\n", result);
    }
    return result;
}

#else

/****************************************************************
 * 所有其他尚未实现周期计数器例程的平台。
 * 较新版本的 sparc（v8plus）具有可以从用户程序
 * 访问的周期计数器，但由于仍有许多 sparc 机器
 * 不支持此功能，我们在此处没有提供 Sparc 版本。
 ***************************************************************/

void start_counter()
{
    printf("ERROR: You are trying to use a start_counter routine in clock.c\n");
    printf("that has not been implemented yet on this platform.\n");
    printf("Please choose another timing package in config.h.\n");
    exit(1);
}

double get_counter()
{
    printf("ERROR: You are trying to use a get_counter routine in clock.c\n");
    printf("that has not been implemented yet on this platform.\n");
    printf("Please choose another timing package in config.h.\n");
    exit(1);
}
#endif





/*******************************
 * 平台无关函数
 ******************************/
double ovhd()
{
    /* 执行两次以消除缓存效应 */
    int i;
    double result;

    for (i = 0; i < 2; i++) {
	start_counter();
	result = get_counter();
    }
    return result;
}

/* $begin mhz */
/* 通过测量在 sleeptime 秒睡眠期间经过的周期数
   来估算时钟频率。 */
double mhz_full(int verbose, int sleeptime)
{
    double rate;

    start_counter();
    sleep(sleeptime);
    rate = get_counter() / (1e6*sleeptime);
    if (verbose)
	printf("Processor clock rate ~= %.1f MHz\n", rate);
    return rate;
}
/* $end mhz */

/* 使用默认睡眠时间的版本 */
double mhz(int verbose)
{
    return mhz_full(verbose, 2);
}

/** 用于补偿定时器中断开销的特殊计数器 */

static double cyc_per_tick = 0.0;

#define NEVENT 100
#define THRESHOLD 1000
#define RECORDTHRESH 3000

/* 尝试查看定时器中断占用了多少时间 */
static void callibrate(int verbose)
{
    double oldt;
    struct tms t;
    clock_t oldc;
    int e = 0;

    times(&t);
    oldc = t.tms_utime;
    start_counter();
    oldt = get_counter();
    while (e <NEVENT) {
	double newt = get_counter();

	if (newt-oldt >= THRESHOLD) {
	    clock_t newc;
	    times(&t);
	    newc = t.tms_utime;
	    if (newc > oldc) {
		double cpt = (newt-oldt)/(newc-oldc);
		if ((cyc_per_tick == 0.0 || cyc_per_tick > cpt) && cpt > RECORDTHRESH)
		    cyc_per_tick = cpt;
		/*
		  if (verbose)
		  printf("Saw event lasting %.0f cycles and %d ticks.  Ratio = %f\n",
		  newt-oldt, (int) (newc-oldc), cpt);
		*/
		e++;
		oldc = newc;
	    }
	    oldt = newt;
	}
    }
    if (verbose)
	printf("Setting cyc_per_tick to %f\n", cyc_per_tick);
}

static clock_t start_tick = 0;

void start_comp_counter()
{
    struct tms t;

    if (cyc_per_tick == 0.0)
	callibrate(0);
    times(&t);
    start_tick = t.tms_utime;
    start_counter();
}

double get_comp_counter()
{
    double time = get_counter();
    double ctime;
    struct tms t;
    clock_t ticks;

    times(&t);
    ticks = t.tms_utime - start_tick;
    ctime = time - ticks*cyc_per_tick;
    /*
      printf("Measured %.0f cycles.  Ticks = %d.  Corrected %.0f cycles\n",
      time, (int) ticks, ctime);
    */
    return ctime;
}

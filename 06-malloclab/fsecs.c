/****************************
 * 高层计时封装
 ****************************/
#include <stdio.h>
#include "fsecs.h"
#include "fcyc.h"
#include "clock.h"
#include "ftimer.h"
#include "config.h"

static double Mhz;  /* 估算的 CPU 时钟频率 */

extern int verbose; /* mdriver.c 中的 -v 选项 */

/*
 * init_fsecs - 初始化计时包
 */
void init_fsecs(void)
{
    Mhz = 0; /* 消除 gcc -Wall 警告 */

#if USE_FCYC
    if (verbose)
	printf("Measuring performance with a cycle counter.\n");

    /* 设置 fcyc 包的关键参数 */
    set_fcyc_maxsamples(20);
    set_fcyc_clear_cache(1);
    set_fcyc_compensate(1);
    set_fcyc_epsilon(0.01);
    set_fcyc_k(3);
    Mhz = mhz(verbose > 0);
#elif USE_ITIMER
    if (verbose)
	printf("Measuring performance with the interval timer.\n");
#elif USE_GETTOD
    if (verbose)
	printf("Measuring performance with gettimeofday().\n");
#endif
}

/*
 * fsecs - 返回函数 f 的运行时间（以秒为单位）
 */
double fsecs(fsecs_test_funct f, void *argp)
{
#if USE_FCYC
    double cycles = fcyc(f, argp);
    return cycles/(Mhz*1e6);
#elif USE_ITIMER
    return ftimer_itimer(f, argp, 10);
#elif USE_GETTOD
    return ftimer_gettod(f, argp, 10);
#endif
}

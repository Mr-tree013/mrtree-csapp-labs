/*
 * ftimer.c - 估算函数 f 所消耗的时间（以秒为单位）
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 *
 * 用于估算函数 f 运行时间（以秒为单位）的函数计时器。
 *    ftimer_itimer: 使用间隔定时器的版本
 *    ftimer_gettod: 使用 gettimeofday 的版本
 */
#include <stdio.h>
#include <sys/time.h>
#include "ftimer.h"

/* 函数原型 */
static void init_etime(void);
static double get_etime(void);

/*
 * ftimer_itimer - 使用间隔定时器估算 f(argp) 的运行时间。
 * 返回 n 次运行的平均值。
 */
double ftimer_itimer(ftimer_test_funct f, void *argp, int n)
{
    double start, tmeas;
    int i;

    init_etime();
    start = get_etime();
    for (i = 0; i < n; i++)
	f(argp);
    tmeas = get_etime() - start;
    return tmeas / n;
}

/*
 * ftimer_gettod - 使用 gettimeofday 估算 f(argp) 的运行时间。
 * 返回 n 次运行的平均值。
 */
double ftimer_gettod(ftimer_test_funct f, void *argp, int n)
{
    int i;
    struct timeval stv, etv;
    double diff;

    gettimeofday(&stv, NULL);
    for (i = 0; i < n; i++)
	f(argp);
    gettimeofday(&etv,NULL);
    diff = 1E3*(etv.tv_sec - stv.tv_sec) + 1E-3*(etv.tv_usec-stv.tv_usec);
    diff /= n;
    return (1E-3*diff);
}


/*
 * 操作 Unix 间隔定时器的例程
 */

/* 间隔定时器的初始值 */
#define MAX_ETIME 86400

/* 保存间隔定时器初始值的静态变量 */
static struct itimerval first_u; /* 用户时间 */
static struct itimerval first_r; /* 真实时间 */
static struct itimerval first_p; /* 概况时间 */

/* 初始化定时器 */
static void init_etime(void)
{
    first_u.it_interval.tv_sec = 0;
    first_u.it_interval.tv_usec = 0;
    first_u.it_value.tv_sec = MAX_ETIME;
    first_u.it_value.tv_usec = 0;
    setitimer(ITIMER_VIRTUAL, &first_u, NULL);

    first_r.it_interval.tv_sec = 0;
    first_r.it_interval.tv_usec = 0;
    first_r.it_value.tv_sec = MAX_ETIME;
    first_r.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &first_r, NULL);

    first_p.it_interval.tv_sec = 0;
    first_p.it_interval.tv_usec = 0;
    first_p.it_value.tv_sec = MAX_ETIME;
    first_p.it_value.tv_usec = 0;
    setitimer(ITIMER_PROF, &first_p, NULL);
}

/* 返回自调用 init_etime 以来经过的真实秒数 */
static double get_etime(void) {
    struct itimerval v_curr;
    struct itimerval r_curr;
    struct itimerval p_curr;

    getitimer(ITIMER_VIRTUAL, &v_curr);
    getitimer(ITIMER_REAL,&r_curr);
    getitimer(ITIMER_PROF,&p_curr);

    return (double) ((first_p.it_value.tv_sec - r_curr.it_value.tv_sec) +
		     (first_p.it_value.tv_usec - r_curr.it_value.tv_usec)*1e-6);
}

#ifndef __CONFIG_H_
#define __CONFIG_H_

/*
 * config.h - malloc lab 配置文件
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

/*
 * 这是驱动程序查找默认 trace 文件的默认路径。
 * 你可以在运行时使用 -t 标志覆盖它。
 */
#define TRACEDIR "/afs/cs/project/ics2/im/labs/malloclab/traces/"

/*
 * 这是 TRACEDIR 中的默认 trace 文件列表，驱动程序将
 * 使用这些文件进行测试。如果你想添加或删除驱动程序
 * 测试套件中的 trace，请修改此列表。例如，如果你不希望
 * 学生实现 realloc，可以删除最后两个 trace。
 */
#define DEFAULT_TRACEFILES \
  "amptjp-bal.rep",\
  "cccp-bal.rep",\
  "cp-decl-bal.rep",\
  "expr-bal.rep",\
  "coalescing-bal.rep",\
  "random-bal.rep",\
  "random2-bal.rep",\
  "binary-bal.rep",\
  "binary2-bal.rep",\
  "realloc-bal.rep",\
  "realloc2-bal.rep"

/*
 * 此常量给出了在某个参考系统（通常与学生使用的
 * 系统类型相同）上，通过我们的 trace 测得的 libc malloc
 * 包的估算性能。其目的是限制 throughput 对性能指数的
 * 贡献。一旦学生超过了 AVG_LIBC_THRUPUT，
 * 他们的分数就不会再提高。这可以阻止学生构建极其快速
 * 但极其低智的 malloc 包。
 */
#define AVG_LIBC_THRUPUT      600E3  /* 600 Kops/sec */

 /*
  * 此常量决定了空间利用率（UTIL_WEIGHT）和
  * throughput（1 - UTIL_WEIGHT）对性能指数的
  * 贡献比例。
  */
#define UTIL_WEIGHT .60

/*
 * alignment 要求（以字节为单位，取 4 或 8）
 */
#define ALIGNMENT 8

/*
 * 最大 heap 大小（以字节为单位）
 */
#define MAX_HEAP (20*(1<<20))  /* 20 MB */

/*****************************************************************************
 * 将以下 USE_xxx 常量中的恰好一个设置为 "1" 以选择计时方法
 *****************************************************************************/
#define USE_FCYC   0   /* 基于周期计数器 + K-best 方案（仅限 x86 和 Alpha） */
#define USE_ITIMER 0   /* 间隔定时器（任何 Unix 机器） */
#define USE_GETTOD 1   /* gettimeofday（任何 Unix 机器） */

#endif /* __CONFIG_H */

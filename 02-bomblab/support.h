#ifndef SUPPORT_H
#define SUPPORT_H

#include <stdio.h>

/* 全局变量 */
extern FILE *infile;             /* 输入源：stdin 或用户指定的文件 */

/* 初始化/反初始化 */
void initialize_bomb(void);      /* 安装 SIGINT handler，让 Ctrl-C 无法终止 bomb */
void initialize_bomb_solve(void);/* （no-op，提交服务器使用） */

/* 输入 */
char *read_line(void);           /* 从 infile 读一行，去掉换行符，存入输入历史 */

/* 字符串工具 */
int string_length(const char *s);
int strings_not_equal(const char *s1, const char *s2);

/* 数值输入 */
int read_six_numbers(char *input, int *arr);  /* sscanf(input, "%d %d %d %d %d %d", ...) */

/* 爆炸 */
void explode_bomb(void) __attribute__((noreturn));
void invalid_phase(const char *s) __attribute__((noreturn));

/* 信号处理 */
void sig_handler(int signum);
void sigalrm_handler(int signum);

/* 阶段完成通知 */
void phase_defused(void);        /* 每关通过后调用；6 关全部通过后触发 secret_phase */

#endif

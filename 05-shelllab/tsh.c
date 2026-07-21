/*
 * tsh - 一个支持作业控制(job control)的微型shell程序
 *
 * <在此处填写你的姓名和登录ID>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* 杂项常量定义 */
#define MAXLINE    1024   /* 最大行长度 */
#define MAXARGS     128   /* 一条命令的最大参数个数 */
#define MAXJOBS      16   /* 任意时刻的最大作业数 */
#define MAXJID    1<<16   /* 最大作业ID */

/* 作业状态 */
#define UNDEF 0 /* 未定义 */
#define FG 1    /* 前台运行 */
#define BG 2    /* 后台运行 */
#define ST 3    /* 已停止 */

/*
 * 作业状态：FG（前台）、BG（后台）、ST（已停止）
 * 作业状态转换及触发操作：
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg 命令
 *     ST -> BG  : bg 命令
 *     BG -> FG  : fg 命令
 * 至多一个作业可以处于FG状态。
 */

/* 全局变量 */
extern char **environ;      /* 定义于libc中 */
char prompt[] = "tsh> ";    /* 命令行提示符（请勿修改） */
int verbose = 0;            /* 若为true，打印额外输出 */
int nextjid = 1;            /* 下一个要分配的作业ID */
char sbuf[MAXLINE];         /* 用于组合sprintf消息 */

struct job_t {              /* 作业结构体 */
    pid_t pid;              /* 作业PID */
    int jid;                /* 作业ID [1, 2, ...] */
    int state;              /* UNDEF、BG、FG 或 ST */
    char cmdline[MAXLINE];  /* 命令行 */
};
struct job_t jobs[MAXJOBS]; /* 作业列表 */
/* 全局变量结束 */


/* 函数原型 */

/* 以下是你需要实现的函数 */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* 以下是我们为你提供的辅助函数 */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - shell的主程序
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* 显示提示符（默认） */

    /* 将stderr重定向到stdout（这样驱动程序可以通过连接到stdout的
     * 管道获取所有输出） */
    dup2(1, 2);

    /* 解析命令行参数 */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* 打印帮助信息 */
            usage();
	    break;
        case 'v':             /* 打印额外的诊断信息 */
            verbose = 1;
	    break;
        case 'p':             /* 不显示命令提示符 */
            emit_prompt = 0;  /* 在自动化测试中很有用 */
	    break;
	default:
            usage();
	}
    }

    /* 安装信号处理函数 */

    /* 以下是需要你实现的信号处理函数 */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* 子进程终止或停止 */

    /* 这个信号处理函数提供了一种优雅终止shell的方式 */
    Signal(SIGQUIT, sigquit_handler);

    /* 初始化作业列表 */
    initjobs(jobs);

    /* 执行shell的读取/求值循环 */
    while (1) {

	/* 读取命令行 */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* 文件结束（ctrl-d） */
	    fflush(stdout);
	    exit(0);
	}

	/* 求值命令行 */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    }

    exit(0); /* 控制流永远无法到达此处 */
}

/*
 * eval - 求值用户刚输入的命令行
 *
 * 如果用户请求的是内置命令（quit、jobs、bg 或 fg），
 * 则立即执行它。否则，fork一个子进程，并在子进程的
 * 上下文中运行该作业。如果作业在前台运行，则等待其
 * 终止然后返回。注意：每个子进程必须拥有唯一的进程组ID，
 * 这样当我们在键盘上键入ctrl-c（ctrl-z）时，内核不会将
 * SIGINT（SIGTSTP）发送给我们的后台子进程。
*/
void eval(char *cmdline)
{
    return;
}

/*
 * parseline - 解析命令行并构建argv数组。
 *
 * 用单引号括起来的字符被视为单个参数。
 * 如果用户请求的是后台作业则返回true，如果用户请求
 * 的是前台作业则返回false。
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* 存放命令行的本地副本 */
    char *buf = array;          /* 遍历命令行的指针 */
    char *delim;                /* 指向第一个空格分隔符 */
    int argc;                   /* 参数个数 */
    int bg;                     /* 是否为后台作业？ */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* 将末尾的'\n'替换为空格 */
    while (*buf && (*buf == ' ')) /* 忽略前导空格 */
	buf++;

    /* 构建argv列表 */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* 忽略空格 */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;

    if (argc == 0)  /* 忽略空行 */
	return 1;

    /* 该作业是否应在后台运行？ */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - 如果用户输入的是内置命令，则立即执行它。
 */
int builtin_cmd(char **argv)
{
    return 0;     /* 不是内置命令 */
}

/*
 * do_bgfg - 执行内置的bg和fg命令
 */
void do_bgfg(char **argv)
{
    return;
}

/*
 * waitfg - 阻塞直到进程pid不再是前台进程
 */
void waitfg(pid_t pid)
{
    return;
}

/*****************
 * 信号处理函数
 *****************/

/*
 * sigchld_handler - 每当一个子作业终止（变为僵尸进程）或
 *     因收到SIGSTOP或SIGTSTP信号而停止时，内核会向shell
 *     发送SIGCHLD信号。该处理函数回收所有可用的僵尸子进程，
 *     但不会等待其他任何当前正在运行的子进程终止。
 */
void sigchld_handler(int sig)
{
    return;
}

/*
 * sigint_handler - 每当用户在键盘上键入ctrl-c时，内核会向
 *     shell发送SIGINT信号。捕获该信号并将其转发给前台作业。
 */
void sigint_handler(int sig)
{
    return;
}

/*
 * sigtstp_handler - 每当用户在键盘上键入ctrl-z时，内核会向
 *     shell发送SIGTSTP信号。捕获该信号并通过向前台作业
 *     发送SIGTSTP来挂起它。
 */
void sigtstp_handler(int sig)
{
    return;
}

/*********************
 * 信号处理函数结束
 *********************/

/***********************************************
 * 操作作业列表的辅助函数
 **********************************************/

/* clearjob - 清除作业结构体的各项条目 */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - 初始化作业列表 */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - 返回已分配的最大作业ID */
int maxjid(struct job_t *jobs)
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - 向作业列表中添加一个作业 */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - 从作业列表中删除PID=pid的作业 */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - 返回当前前台作业的PID，若无则返回0 */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - 在作业列表中按PID查找作业 */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - 在作业列表中按JID查找作业 */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - 将进程ID映射为作业ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - 打印作业列表 */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG:
		    printf("Running ");
		    break;
		case FG:
		    printf("Foreground ");
		    break;
		case ST:
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ",
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * 作业列表辅助函数结束
 ******************************/


/***********************
 * 其他辅助函数
 ***********************/

/*
 * usage - 打印帮助信息
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix风格的错误处理函数
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - 应用程序风格的错误处理函数
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - sigaction函数的包装器
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* 在处理该类型信号时阻塞它本身 */
    action.sa_flags = SA_RESTART; /* 如果可能则重启系统调用 */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - 驱动程序可以通过向子shell发送SIGQUIT信号
 *     来优雅地终止它。
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



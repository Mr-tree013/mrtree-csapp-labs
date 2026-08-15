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
    char *argv[MAXARGS];      /* 解析后的参数数组，argv[0] 为命令名 */
    char buf[MAXLINE];        /* cmdline 的本地副本，供 parseline 破坏式解析 */
    int bg;                   /* 1=后台作业，0=前台作业 */
    pid_t pid;                /* fork 出的子进程 PID */
    sigset_t mask_one, prev_one; /* mask_one: 仅含 SIGCHLD；prev_one: 修改前的掩码 */

    /* parseline 会把 buf 里的分隔符改成 '\0'，所以要复制一份，保留原始 cmdline */
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    /* 空行或孤立的 '&'：parseline 解析后 argc=0（argv[0]==NULL），直接忽略 */
    if (argv[0] == NULL)
        return;

    /* 内置命令（quit/jobs/bg/fg）不 fork，直接在当前 shell 进程内执行 */
    if (!builtin_cmd(argv)) {

        /*
         * 竞态条件处理（本 lab 最核心的一点）：
         * 若不先屏蔽 SIGCHLD 就 fork，子进程可能在父进程执行 addjob 之前
         * 就已终止并向父进程投递 SIGCHLD。此时 sigchld_handler 会尝试
         * deletejob 一个尚未加入列表的作业（删除失败、什么都不做）；随后
         * 父进程又把这个"已经死掉"的作业 addjob 进列表 —— 结果列表里
         * 残留一个永远无法回收的僵尸作业。
         *
         * 解法：fork 之前屏蔽 SIGCHLD，等父进程安全完成 addjob 后再恢复。
         * 期间即使子进程很快退出，SIGCHLD 也只是处于 pending（挂起）状态，
         * 直到 addjob 完成、恢复掩码后才被投递给 handler。
         */
        sigemptyset(&mask_one);
        sigaddset(&mask_one, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask_one, &prev_one);

        if ((pid = fork()) == 0) {
            /* ---- 子进程分支 ---- */

            /* 恢复信号掩码：子进程不应继承"SIGCHLD 被屏蔽"这个临时状态 */
            sigprocmask(SIG_SETMASK, &prev_one, NULL);

            /*
             * 让子进程自成新的进程组，组 ID = 子进程 PID。
             * 这样父进程用 kill(-pid, sig) 就能把信号发给整组进程，
             * 既不会误伤 shell 自己，也不会影响其它后台作业。
             */
            setpgid(0, 0);

            /* 用新程序覆盖子进程映像；失败则打印错误并退出（不能继续 shell 循环） */
            if (execve(argv[0], argv, environ) < 0) {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
        }

        /* ---- 父进程分支 ---- */

        /* 此刻 SIGCHLD 仍被屏蔽，addjob 是安全的：前台记 FG，后台记 BG */
        if (!bg)
            addjob(jobs, pid, FG, cmdline);
        else
            addjob(jobs, pid, BG, cmdline);

        /* 作业已安全入列，恢复原信号掩码（解除对 SIGCHLD 的屏蔽） */
        sigprocmask(SIG_SETMASK, &prev_one, NULL);

        if (!bg) {
            /* 前台作业：阻塞等待它结束（期间由 sigchld_handler 负责回收并删除） */
            waitfg(pid);
        }
        else {
            /* 后台作业：不等待，打印作业信息后立即返回，继续读下一条命令 */
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
    }
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
    /* quit：立即退出 shell */
    if (!strcmp(argv[0], "quit"))
        exit(0);

    /* 孤立的 '&'：视为空命令，直接吞掉（返回 1 表示"已处理，不再 fork"） */
    if (!strcmp(argv[0], "&"))
        return 1;

    /* jobs：打印作业列表 */
    if (!strcmp(argv[0], "jobs")) {
        listjobs(jobs);
        return 1;
    }

    /* bg / fg：交给 do_bgfg 处理 */
    if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        do_bgfg(argv);
        return 1;
    }

    /* 不是内置命令，返回 0，让 eval 走 fork + execve 路径 */
    return 0;
}

/*
 * do_bgfg - 执行内置的bg和fg命令
 */
void do_bgfg(char **argv)
{
    struct job_t *job;   /* 目标作业 */
    int id;              /* 解析出的 PID 或 JID */
    pid_t pid;

    /* bg/fg 必须带参数 */
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    /* 参数以 '%' 开头 → 按作业 ID（JID）解析；否则按进程 ID（PID）解析 */
    if (argv[1][0] == '%') {
        id = atoi(&argv[1][1]);       /* 跳过 '%'，把剩余部分转成数字 */
        if (id <= 0) {                /* atoi 对非数字返回 0，JID 从 1 开始 */
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        job = getjobjid(jobs, id);
        if (job == NULL) {
            printf("%%%d: No such job\n", id);
            return;
        }
    }
    else {
        id = atoi(argv[1]);
        if (id <= 0) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        job = getjobpid(jobs, id);
        if (job == NULL) {
            printf("(%d): No such process\n", id);
            return;
        }
    }

    pid = job->pid;

    /*
     * 向作业所在进程组发送 SIGCONT，唤醒被停止的进程。
     * 注意第一个参数是 -pid（负数）：负数在 kill 中表示"进程组"而非单个进程。
     * 因为一个作业可能由多个进程组成（例如 mysplit 会 fork 子进程），
     * 只对单个 pid 发信号无法唤醒整组，必须对整个进程组广播。
     */
    kill(-pid, SIGCONT);

    if (!strcmp(argv[0], "bg")) {
        /* 转为后台：改状态并打印，不等待 */
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }
    else { /* fg */
        /* 转为前台：改状态后阻塞等待它结束/被停止 */
        job->state = FG;
        waitfg(pid);
    }
    return;
}

/*
 * waitfg - 阻塞直到进程pid不再是前台进程
 */
void waitfg(pid_t pid)
{
    sigset_t mask, prev;

    /*
     * 阻塞等待，直到 pid 不再处于前台（被回收删除，或被 Ctrl-Z 停止）。
     *
     * 实现要点——用 sigsuspend 原子挂起，而不是忙等或 sleep：
     *   忙等 while(fgpid(jobs)==pid); 会空转占满 CPU；
     *   sleep 则要轮询，响应有延迟且时间粒度粗。
     *   sigsuspend 能原子地"替换信号掩码 + 挂起"，当 SIGCHLD 到达时被唤醒，
     *   与"子进程被回收"这件事精确同步。
     *
     * 为什么先屏蔽 SIGCHLD 再进入循环？
     *   这是"检查-再挂起"的经典写法，用来消除竞态：
     *   若先检查 fgpid、再调用 sigsuspend，两次操作之间可能恰好漏掉一次
     *   SIGCHLD（信号在中间到达，handler 已删掉作业），随后 sigsuspend
     *   却再也等不到下一个信号，导致 shell 永久阻塞。
     *   先屏蔽 SIGCHLD 后：进入 sigsuspend 前信号只会 pending；
     *   一旦在 sigsuspend 中临时解除屏蔽，pending 的 SIGCHLD 立即投递、
     *   handler 回收子进程并删除作业，sigsuspend 返回，循环再检查
     *   fgpid 便发现它已经退场。
     */
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);   /* prev 记录旧掩码（此时 SIGCHLD 未屏蔽） */

    while (fgpid(jobs) == pid) {
        /* 临时用 prev 替换掩码（SIGCHLD 恢复未屏蔽）并挂起，直到有信号到达 */
        sigsuspend(&prev);
    }

    sigprocmask(SIG_SETMASK, &prev, NULL);
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
    int olderrno = errno;          /* 保存 errno，结束时恢复，避免干扰主流程 */
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    /* 屏蔽所有信号后再操作共享的作业列表 jobs[]，防止与其它 handler
     * 或主流程并发修改 jobs 造成数据不一致 */
    sigfillset(&mask_all);

    /*
     * 循环回收子进程。waitpid 参数含义：
     *   -1       : 回收"任意"子进程（而不只是一个）
     *   WNOHANG  : 非阻塞，没有已终止/停止的子进程时立即返回 0，不卡住 shell
     *   WUNTRACED: 除了"终止"，也报告"被停止"的子进程（Ctrl-Z 正需要它）
     * 返回值 >0 表示回收到一个子进程；<=0 表示没有更多可回收的，循环结束。
     */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        if (WIFEXITED(status)) {
            /* 子进程正常退出：从作业列表删除 */
            deletejob(jobs, pid);
        }
        else if (WIFSIGNALED(status)) {
            /* 子进程被信号杀死：打印信息，并从作业列表删除 */
            printf("Job [%d] (%d) terminated by signal %d\n",
                   pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }
        else if (WIFSTOPPED(status)) {
            /* 子进程被信号停止（Ctrl-Z 或 mystop）：打印信息，状态改为 ST，
             * 但【不删除】——它只是暂停，之后可用 bg/fg 唤醒 */
            printf("Job [%d] (%d) stopped by signal %d\n",
                   pid2jid(pid), pid, WSTOPSIG(status));
            getjobpid(jobs, pid)->state = ST;
        }

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    errno = olderrno;
    return;
}

/*
 * sigint_handler - 每当用户在键盘上键入ctrl-c时，内核会向
 *     shell发送SIGINT信号。捕获该信号并将其转发给前台作业。
 */
void sigint_handler(int sig)
{
    int olderrno = errno;
    pid_t pid;

    /* 取当前前台作业的 PID；没有前台作业时 fgpid 返回 0 */
    pid = fgpid(jobs);

    /*
     * 用 kill(-pid, SIGINT) 把 SIGINT 转发给整个前台进程组。
     * 负数 pid 表示"进程组"：组内所有进程（含 fork 出的子进程）都收到，
     * 从而整体终止，而不是只有组长进程被杀死。
     */
    if (pid != 0)
        kill(-pid, SIGINT);

    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - 每当用户在键盘上键入ctrl-z时，内核会向
 *     shell发送SIGTSTP信号。捕获该信号并通过向前台作业
 *     发送SIGTSTP来挂起它。
 */
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    pid_t pid;

    /* 取当前前台作业的 PID；没有前台作业时 fgpid 返回 0 */
    pid = fgpid(jobs);

    /*
     * 用 kill(-pid, SIGTSTP) 把 SIGTSTP 转发给整个前台进程组，
     * 让前台作业（连同其子进程）整体挂起。
     * 实际的"状态改 ST"由随后到达的 SIGCHLD → sigchld_handler 完成：
     * 被停止的子进程会触发 waitpid 的 WUNTRACED 返回。
     */
    if (pid != 0)
        kill(-pid, SIGTSTP);

    errno = olderrno;
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



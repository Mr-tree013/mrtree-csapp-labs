# Shell Lab 讲解文档

> 对应源码：`tsh.c`。本实验实现一个支持「作业控制」（job control）的微型 Unix Shell，名为 `tsh`（Tiny Shell）。

---

## 一、这个实验到底在做什么

普通的 shell 能做的事很多，`tsh` 只要求实现其中最关键、也最能体现操作系统「异常控制流」思想的一小部分：

1. **执行程序**：用户在提示符后输入一条命令，shell 要能 `fork` 一个子进程，用 `execve` 把它替换成用户想运行的程序。
2. **前台 / 后台**：命令末尾带 `&` 就是后台作业——shell 不等待、立刻回到提示符；不带 `&` 就是前台作业——shell 阻塞等待它跑完。
3. **作业控制内置命令**：
   - `jobs`：列出当前所有作业及其状态；
   - `bg %N` / `bg PID`：把一个**已停止**的作业放到后台继续跑；
   - `fg %N` / `fg PID`：把一个作业（停止的或后台的）调到前台；
   - `quit`：退出 shell。
4. **信号转发**：
   - 用户按 `Ctrl-C`（SIGINT）→ shell 把 SIGINT 转发给**前台作业的整个进程组**；
   - 用户按 `Ctrl-Z`（SIGTSTP）→ shell 把 SIGTSTP 转发给前台进程组，让它整体挂起；
   - 子进程终止或停止 → 内核给 shell 发 SIGCHLD，shell 要回收僵尸进程并更新作业状态。

一句话总结：**这是 CS:APP 第 8 章（异常控制流）的集大成练习，把 `fork` / `execve` / `waitpid` / 信号 / 进程组 / 竞态条件全部串起来了。**

---

## 二、整体架构

`tsh` 是一个单进程程序，围绕一个**全局作业列表** `jobs[]` 运转。数据流如下：

```
main 主循环（读取-求值循环）
   │
   ├─ 读到一行命令 ──> eval(cmdline)
   │                     │
   │                     ├─ 是内置命令? ──> builtin_cmd / do_bgfg（不 fork，直接执行）
   │                     │
   │                     └─ 是外部命令? ──> 屏蔽SIGCHLD → fork
   │                                            │
   │                          ┌─────────────────┴─────────────────┐
   │                       父进程                             子进程
   │                     addjob(入列)                    setpgid(0,0) 自成进程组
   │                     解除SIGCHLD屏蔽                  execve 替换映像
   │                     ├─ 前台 → waitfg(阻塞等待)
   │                     └─ 后台 → 打印作业信息，立即返回
   │
   └─ 信号处理（异步，随时打断主流程）:
         sigint_handler   : Ctrl-C → kill(-fgpid, SIGINT)
         sigtstp_handler  : Ctrl-Z → kill(-fgpid, SIGTSTP)
         sigchld_handler  : 子进程终止/停止 → waitpid 回收 + 更新 jobs[]
```

关键点：**主循环和信号处理函数是并发运行的**（信号随时打断主流程），它们共享同一份 `jobs[]`。所以整份代码的正确性，本质上是「如何安全地读写这份共享数据」的问题。

---

## 三、七个需要实现的函数逐个讲解

### 3.1 `eval` —— 主逻辑核心

```c
void eval(char *cmdline)
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;
    sigset_t mask_one, prev_one;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);       // 解析命令行，bg=1 表示后台
    if (argv[0] == NULL)
        return;                      // 空行 / 孤立 '&'，直接忽略

    if (!builtin_cmd(argv)) {        // 不是内置命令才 fork

        sigemptyset(&mask_one);
        sigaddset(&mask_one, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask_one, &prev_one);   // ① 屏蔽 SIGCHLD

        if ((pid = fork()) == 0) {   // ② fork
            sigprocmask(SIG_SETMASK, &prev_one, NULL);  // 子进程恢复掩码
            setpgid(0, 0);                              // 子进程自成进程组
            if (execve(argv[0], argv, environ) < 0) {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
        }

        if (!bg)                     // ③ 父进程把作业入列（此刻 SIGCHLD 仍被屏蔽）
            addjob(jobs, pid, FG, cmdline);
        else
            addjob(jobs, pid, BG, cmdline);

        sigprocmask(SIG_SETMASK, &prev_one, NULL);      // ④ 解除屏蔽

        if (!bg)
            waitfg(pid);             // 前台：阻塞等待
        else
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);  // 后台：打印并返回
    }
    return;
}
```

**为什么要在 fork 前屏蔽 SIGCHLD？** 这是整个实验**最核心的竞态条件**，必须彻底理解：

> 假设不屏蔽。子进程可能非常快——`fork` 刚返回，父进程还没来得及 `addjob`，子进程就已经 `execve` 失败 / 运行结束，并向父进程投递 SIGCHLD。此时 `sigchld_handler` 被触发，它调用 `deletejob(jobs, pid)` 想删掉这个作业，但作业**还没入列**，所以删除失败、什么都没删。随后父进程才执行 `addjob`，把一个「已经死掉的进程」加进列表。结果：`jobs[]` 里永远残留一个僵尸作业，`jobs` 命令会显示一个根本不存在的作业。

**解决办法**：在 `fork` 之前屏蔽 SIGCHLD。这样即使子进程早早退出，SIGCHLD 也只是「挂起」（pending），不会立刻投递给 handler。等父进程安全完成 `addjob`、解除屏蔽后，SIGCHLD 才被投递，此时作业已经在列表里，`deletejob` 就能正确删除。

这个「屏蔽 → 操作共享数据 → 恢复」的模式，是处理**信号与主流程共享数据**的标准手法，后面 `sigchld_handler` 里还会再用到。

**为什么子进程要 `setpgid(0, 0)`？** 让子进程成为一个**新进程组的组长**（进程组 ID = 子进程 PID）。这样父进程以后可以用 `kill(-pid, 信号)` 把信号发给**整个组**，而不是只发给这一个进程。这一点在 `mysplit`（会 fork 子进程）上尤其重要：`Ctrl-C` 必须同时杀死父子两个进程，否则会留下孤儿进程。

---

### 3.2 `builtin_cmd` —— 内置命令分发

```c
int builtin_cmd(char **argv)
{
    if (!strcmp(argv[0], "quit"))
        exit(0);                     // 退出整个 shell
    if (!strcmp(argv[0], "&"))
        return 1;                    // 孤立 '&'，当空命令吞掉
    if (!strcmp(argv[0], "jobs")) {
        listjobs(jobs);              // 打印作业列表
        return 1;
    }
    if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        do_bgfg(argv);               // 交给 do_bgfg 处理
        return 1;
    }
    return 0;                        // 不是内置命令，eval 会去 fork
}
```

**返回值语义**：返回 `1` 表示「这是内置命令，我已经处理了，`eval` 不用再 fork」；返回 `0` 表示「不是内置命令，`eval` 走 fork + execve」。`quit` 直接 `exit(0)`，所以永远不会返回。

---

### 3.3 `do_bgfg` —— bg / fg 的实现

```c
void do_bgfg(char **argv)
{
    struct job_t *job;
    int id;
    pid_t pid;

    if (argv[1] == NULL) {                                   // 缺参数
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    if (argv[1][0] == '%') {                                 // '%N' → 按作业 ID 查
        id = atoi(&argv[1][1]);
        if (id <= 0) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
        job = getjobjid(jobs, id);
        if (job == NULL) {
            printf("%%%d: No such job\n", id);
            return;
        }
    }
    else {                                                   // 数字 → 按 PID 查
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

    kill(-pid, SIGCONT);                                     // 唤醒整个进程组

    if (!strcmp(argv[0], "bg")) {
        job->state = BG;                                     // 转后台，不等待
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }
    else {
        job->state = FG;                                     // 转前台
        waitfg(pid);                                         // 阻塞等待
    }
    return;
}
```

**三个要点**：

1. **参数区分**：`%N` 表示作业 ID（JID），纯数字表示进程 ID（PID）。JID 是 shell 自己编的、从 1 开始的小整数；PID 是操作系统分配的。作业列表里两者都有，所以两种查法都要支持。
2. **`kill(-pid, SIGCONT)` 里的负号**：`kill` 的第一个参数为正数 → 发给单个进程；为负数 → 发给整个进程组（组 ID = `|参数|`）。因为作业可能由多个进程组成（如 `mysplit` fork 出父子两个），必须对整个组广播 SIGCONT 才能一起唤醒。
3. **`fg` 之后要 `waitfg`**：调到前台意味着 shell 要把控制权交给它、等它结束（或被再次停止）才能回到提示符。

---

### 3.4 `waitfg` —— 阻塞等待前台作业

```c
void waitfg(pid_t pid)
{
    sigset_t mask, prev;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);   // 屏蔽 SIGCHLD

    while (fgpid(jobs) == pid) {            // 只要它还是前台作业，就挂起等待
        sigsuspend(&prev);                  // 原子地"恢复掩码 + 挂起"
    }

    sigprocmask(SIG_SETMASK, &prev, NULL);  // 恢复掩码
    return;
}
```

**为什么不用 `while (fgpid(jobs) == pid) ;` 忙等？** 忙等会占满一个 CPU 核心空转，而且没有任何同步机制，浪费且不优雅。

**为什么用 `sigsuspend` 而不是 `sleep`？** `sigsuspend(&prev)` 会**原子地**完成两件事：把信号掩码临时替换成 `prev`（其中 SIGCHLD 是解除屏蔽的），然后挂起进程，直到有信号到达才返回。这样「等前台作业结束」这件事就和「子进程被回收」精确同步——SIGCHLD 一到，handler 回收子进程、删除作业，`sigsuspend` 返回，循环再检查 `fgpid` 发现它已经不是前台作业了，退出。

**为什么要先屏蔽 SIGCHLD 再进循环？** 这是经典的「检查-再挂起」（test-then-suspend）竞态防护：

> 如果先检查 `fgpid(jobs) == pid`、再调用 `sigsuspend`，两步之间可能恰好漏掉一次 SIGCHLD——信号在两步之间到达，handler 已经把作业删了，随后 `sigsuspend` 却再也等不到下一个信号，shell 就永久卡死。

先屏蔽 SIGCHLD，保证进入 `sigsuspend` 之前信号只会「挂起」；一旦在 `sigsuspend` 里解除屏蔽，挂起的 SIGCHLD 立刻投递、handler 删作业、`sigsuspend` 返回，循环再检查 `fgpid` 就发现作业退场了。这样无论信号在哪个瞬间到达都不会漏。

---

### 3.5 `sigchld_handler` —— 回收子进程

```c
void sigchld_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    sigfillset(&mask_all);   // 屏蔽所有信号，保护共享的 jobs[]

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        if (WIFEXITED(status)) {                 // 正常退出 → 删除
            deletejob(jobs, pid);
        }
        else if (WIFSIGNALED(status)) {          // 被信号杀死 → 打印 + 删除
            printf("Job [%d] (%d) terminated by signal %d\n",
                   pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }
        else if (WIFSTOPPED(status)) {           // 被信号停止 → 打印 + 改 ST
            printf("Job [%d] (%d) stopped by signal %d\n",
                   pid2jid(pid), pid, WSTOPSIG(status));
            getjobpid(jobs, pid)->state = ST;
        }

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    errno = olderrno;
    return;
}
```

**`waitpid` 三个参数的含义是本函数的核心**：

| 参数 | 值 | 含义 |
|------|-----|------|
| pid | `-1` | 回收**任意**一个子进程（不指定具体某个） |
| options | `WNOHANG` | **非阻塞**：没有已终止/停止的子进程时立即返回 0，绝不让 shell 卡住 |
| options | `WUNTRACED` | 除了「终止」，也报告「被停止」的子进程（`Ctrl-Z` 正需要它） |

**三种 `status` 的区分**（用宏 `WIFEXITED` / `WIFSIGNALED` / `WIFSTOPPED` 判断）：

1. **正常退出**（`WIFEXITED`）：比如 `myspin 1` 睡满 1 秒后 `exit(0)`。静默地从列表删除即可，不用打印。
2. **被信号杀死**（`WIFSIGNALED`）：比如 `Ctrl-C` 杀死前台作业。要打印 `terminated by signal 2`（2 就是 SIGINT 的编号），然后删除。
3. **被信号停止**（`WIFSTOPPED`）：比如 `Ctrl-Z` 挂起前台作业。要打印 `stopped by signal 20`（20 是 SIGTSTP），**但不能删除**——作业只是暂停，之后还能用 `bg`/`fg` 唤醒，所以只把状态改成 `ST`。

**为什么要循环 + `WNOHANG`？** 一个 SIGCHLD 可能对应多个同时终止的子进程（例如后台同时跑了好几个作业，几乎同时结束）。`waitpid(-1, WNOHANG|WUNTRACED)` 一次只能回收一个，所以要用 `while` 循环一直回收，直到返回 0（没有更多了）为止。

**为什么屏蔽所有信号？** `jobs[]` 是主流程和所有信号处理函数共享的数据。handler 里要修改它（`deletejob`、`getjobpid(...)->state = ST`），为避免和 `sigint_handler` / `sigtstp_handler` / 主流程并发修改造成数据不一致，先把所有信号屏蔽，改完再恢复。

---

### 3.6 `sigint_handler` 与 `sigtstp_handler` —— 转发 Ctrl-C / Ctrl-Z

```c
void sigint_handler(int sig)
{
    int olderrno = errno;
    pid_t pid = fgpid(jobs);          // 取前台作业 PID，无则 0

    if (pid != 0)
        kill(-pid, SIGINT);           // 转发给整个前台进程组

    errno = olderrno;
    return;
}
```

`sigtstp_handler` 完全同构，只是把 `SIGINT` 换成 `SIGTSTP`。

**关键点**：

1. **只转发给「前台」作业**：用 `fgpid(jobs)` 拿到当前前台作业的 PID。如果此刻没有前台作业（用户在提示符下按 Ctrl-C），`fgpid` 返回 0，什么都不做——这是必须的，否则后台作业会被误杀。
2. **`kill(-pid, ...)` 再次用到负号**：转发给整个进程组，保证 `mysplit` 这种多进程作业被整体杀死/挂起。
3. **只发信号、不改状态**：handler 里只负责 `kill`。作业状态的更新（删除 / 改成 ST）交给随后到达的 SIGCHLD → `sigchld_handler` 完成。这是清晰的职责划分：**发送信号是"请求"，子进程状态的最终落点统一由 `sigchld_handler` 记录**。

---

## 四、几个必须吃透的核心概念

### 4.1 进程组（process group）

- 每个进程都属于某个进程组，组 ID 是某个进程的 PID（通常就是组长进程）。
- shell 用进程组来**批量控制**一组进程：一次 `kill(-pgid, sig)` 就能给整组发信号。
- `setpgid(0, 0)` 让子进程自己成为新组的组长。
- 判断依据：`kill` 的 pid 参数为负 → 目标是进程组，取绝对值作为组 ID。

### 4.2 信号与信号屏蔽

- **信号**是内核/其他进程发给进程的「异步通知」。
- **屏蔽（block）**一个信号 ≠ 丢弃它：被屏蔽的信号会「挂起」（pending），一旦解除屏蔽就会投递。这正是 `eval` 和 `waitfg` 里竞态防护所依赖的性质。
- `sigprocmask`：修改当前进程的信号屏蔽集。
- `sigsuspend`：原子地「替换屏蔽集 + 挂起直到信号到达」。

### 4.3 僵尸进程与回收

- 子进程终止后，如果父进程不 `waitpid` 回收，它会变成**僵尸进程**（占用一个进程表项，无法真正消失）。
- shell 必须通过 `sigchld_handler` 里的 `waitpid(-1, WNOHANG|WUNTRACED)` 持续回收，否则僵尸会越积越多。

### 4.4 竞态条件（race condition）

- 当「异步事件（信号）」和「主流程」访问同一份共享数据时，事件发生的**先后顺序**可能影响结果。
- 本实验的两处典型竞态：
  1. `eval` 里「子进程在 `addjob` 前就退出」→ 用「屏蔽 SIGCHLD 直到 addjob 完成」解决。
  2. `waitfg` 里「SIGCHLD 在检查与挂起之间到达」→ 用「屏蔽 SIGCHLD + sigsuspend」解决。

---

## 五、关键不变量（判断实现对错的依据）

1. **任一已存在的子进程，要么尚未加入 `jobs[]` 且 SIGCHLD 被屏蔽，要么已安全加入 `jobs[]`。**（这是防竞态的根本。）
2. 前台作业至多一个；它的状态一定是 `FG`。
3. 被 `Ctrl-Z` 停止的作业状态必须是 `ST`，且**保留在列表中**（不删除）。
4. 正常退出 / 被信号杀死的作业必须从列表**删除**。
5. 所有发给「作业」的信号都用 `kill(-pid, ...)` 对整个进程组广播。

---

## 六、如何测试

```bash
make                 # 编译 tsh 和 myspin/mysplit/mystop/myint
make test01          # 跑第 1 个 trace（用你的 tsh）
make rtest01         # 跑第 1 个 trace（用参考实现 tshref，用于对照）
```

`sdriver.pl` 是驱动脚本：它把 `tsh` 作为子进程跑起来，按 `traceNN.txt` 里的指令发送命令和信号（`TSTP`=Ctrl-Z，`INT`=Ctrl-C，`SLEEP`=暂停等），再捕获输出。判断对错的方式是**把你的输出和 `tshref` 的输出逐字对比**（需要把每次运行都不同的 PID 归一化掉）。

更直接的对拍命令：

```bash
# 归一化 PID 后对比某个 trace 的两种输出
./sdriver.pl -t trace14.txt -s ./tsh    -a "-p" 2>&1 | sed -E 's/\([0-9]+\)/(PID)/g' > a.txt
./sdriver.pl -t trace14.txt -s ./tshref -a "-p" 2>&1 | sed -E 's/\([0-9]+\)/(PID)/g' > b.txt
diff a.txt b.txt
```

---

## 七、本实现里值得注意的两处「工程简化」

这两处不是 bug，是 CS:APP 官方解法为教学而接受的取舍，但你应该知道它们的存在：

1. **信号处理函数里用了 `printf`**：`printf` 不是异步信号安全（async-signal-safe）的函数。理论上，如果主流程恰好正在 `printf`（持有了 stdio 的锁），信号处理函数里再 `printf` 可能死锁。教科书上「正确」的做法是用 `write` 这类系统调用。这里沿用官方解法用 `printf`，在实际测试中窗口极小、不会触发，但这是需要记住的隐患。

2. **`setpgid` 只在子进程里调用**：严格来说，父进程和子进程都应该调用 `setpgid`（避免「父进程在子进程完成 `setpgid` 之前就向它发信号」的竞态）。这里沿用官方解法只在子进程调用，因为信号发送（Ctrl-C/Ctrl-Z）发生在子进程早已完成 `setpgid` 之后，测试中不会触发竞态。

---

## 八、建议的复盘问题

学完后试着回答这些问题，能答上来才算真正掌握：

1. 如果把 `eval` 里「屏蔽 SIGCHLD」那两行删掉，跑哪个 trace 会出现问题？现象是什么？
2. `waitfg` 里如果把 `sigsuspend` 换成 `sleep(1)` 轮询，结果还对不对？有没有效率或时序问题？
3. `sigchld_handler` 里如果把 `WNOHANG` 去掉（变成阻塞 waitpid），会发生什么？
4. `do_bgfg` 里 `kill(-pid, SIGCONT)` 如果误写成 `kill(pid, SIGCONT)`（少个负号），对 `mysplit` 作业会有什么影响？
5. 为什么 `sigint_handler` 要先 `fgpid(jobs)` 判断是否为 0，而不是无条件 `kill(-1, SIGINT)` 发给所有进程？

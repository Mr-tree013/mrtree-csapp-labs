# Shell Lab

实现一个简易 Unix Shell (`tsh` — Tiny Shell)，支持作业控制和信号处理。

## 目标

- 理解进程控制：`fork`、`execve`、`waitpid`
- 理解信号处理：`sigaction`、信号阻塞、`SIGCHLD`、`SIGINT`、`SIGTSTP`
- 理解进程组与终端作业控制

## 功能要求

- 执行前台/后台命令
- 内置命令：`quit`、`jobs`、`bg <job>`、`fg <job>`
- `SIGINT` (Ctrl-C) 转发给前台作业
- `SIGTSTP` (Ctrl-Z) 挂起前台作业
- 正确处理子进程终止和停止信号

## 文件

- `tsh.c` — Tiny Shell 实现

## 参考资料

- CS:APP3e 第 8 章
- [Shell Lab 说明](http://csapp.cs.cmu.edu/3e/shlab.pdf)

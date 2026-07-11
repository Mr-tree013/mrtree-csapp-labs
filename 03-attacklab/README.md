# Attack Lab

利用缓冲区溢出漏洞进行代码注入攻击（Code Injection）和面向返回编程攻击（ROP）。

## 目标

- 理解栈帧结构与缓冲区溢出原理
- 掌握代码注入攻击技术
- 理解 ROP (Return-Oriented Programming) 攻击原理
- 理解栈金丝雀（stack canary）、ASLR、NX 位等防御机制

## 目标程序

- `ctarget` — Code Injection 攻击目标（无栈随机化、无可执行栈保护）
- `rtarget` — ROP 攻击目标（有栈随机化、NX 位启用）

## 文件

- `ctarget-solutions.txt` — code injection 阶段 1-3 的攻击字符串（十六进制）
- `rtarget-solutions.txt` — ROP 阶段 4-5 的攻击字符串（十六进制）

## 参考资料

- CS:APP3e 第 3.10.3-3.10.4 节
- [Attack Lab 说明](http://csapp.cs.cmu.edu/3e/attacklab.pdf)

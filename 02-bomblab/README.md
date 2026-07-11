# Bomb Lab

通过逆向工程拆解"炸弹"（二进制可执行文件），每个阶段需要输入正确的字符串才能拆除。

## 目标

- 熟练使用 GDB 进行 x86-64 汇编级调试
- 理解函数调用约定、栈帧、条件分支的底层实现
- 解读反汇编代码中的控制流与数据流

## 工具

- `gdb` / `objdump` — 反汇编与调试
- 可能需要 `strings`、`gdb` 断点与单步执行

## 阶段

bomb 程序包含 6 个 phase（外加一个隐藏 phase），每个需要输入特定字符串。

## 文件

- `solutions.txt` — 各 phase 的解谜答案

## 参考资料

- CS:APP3e 第 3 章（机器级表示）
- GDB 快速参考
- [Bomb Lab 说明](http://csapp.cs.cmu.edu/3e/bomblab.pdf)

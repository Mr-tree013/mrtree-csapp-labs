# Cache Lab

实现一个缓存模拟器并使用缓存优化技术加速矩阵运算。

## 目标

- 理解缓存结构（组相联、行、块）
- 实现 LRU 替换策略的缓存模拟器
- 通过矩阵分块（blocking）优化缓存命中率

## Part A: 缓存模拟器 (`csim.c`)

模拟一个组相联缓存（可配置 s, E, b），实现 LRU 替换策略。

## Part B: 矩阵转置优化 (`trans.c`)

编写不同尺寸矩阵的最优转置函数，目标是最小化缓存未命中次数。

## 文件

- `csim.h` — 缓存模拟器头文件
- `csim.c` — 缓存模拟器实现

## 参考资料

- CS:APP3e 第 6 章
- [Cache Lab 说明](http://csapp.cs.cmu.edu/3e/cachelab.pdf)

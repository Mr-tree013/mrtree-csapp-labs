# Proxy Lab

实现一个并发 HTTP 代理服务器，支持 HTTP GET 请求转发与缓存。

## 目标

- 理解 HTTP 协议（请求/响应格式）
- 掌握并发编程（多线程或 I/O 多路复用）
- 理解 Web 缓存机制
- 正确处理客户端-代理-服务器三方通信

## 功能要求

### Part A: 顺序代理
- 接受 HTTP GET 请求
- 转发到目标服务器
- 将响应返回给客户端

### Part B: 并发代理
- 使用 `pthread` 实现并发处理多个客户端

### Part C: 带缓存的并发代理
- 缓存最近访问的 Web 对象
- 使用简单的 LRU 替换策略

## 文件

- `proxy.c` — HTTP 代理实现

## 注意事项

- 端口号从命令行参数读取
- 正确处理 HTTP/1.0 和 HTTP/1.1
- 注意信号处理（SIGPIPE）

## 参考资料

- CS:APP3e 第 10-12 章
- HTTP/1.0 RFC 1945
- [Proxy Lab 说明](http://csapp.cs.cmu.edu/3e/proxylab.pdf)

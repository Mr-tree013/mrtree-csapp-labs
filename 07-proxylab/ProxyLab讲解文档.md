# Proxy Lab 讲解文档

> 对应源码：`proxy.c`。本实验实现一个支持「并发 + 缓存」的 HTTP 代理服务器。
> 分三部分：Part A 顺序代理、Part B 多线程并发、Part C LRU 缓存。

---

## 一、这个实验在做什么

浏览器/curl 访问网页时，有时不是直接连目标服务器，而是先连一个**代理服务器**，由代理代为转发请求、取回响应。本实验就是实现这样一个代理：

```
客户端(curl)  ──①请求──►  代理(proxy)  ──②转发──►  源服务器(tiny)
     ▲                        │  ▲                        │
     └──────④响应─────────────┘  └────────③响应────────────┘
```

代理的三项核心能力，对应三个 Part：

| Part | 能力 | 分值 |
|------|------|------|
| A | 顺序代理：转发 GET 请求、回传响应（含二进制） | 40 |
| B | 并发：每连接一线程，慢客户端不阻塞其他客户端 | 15 |
| C | 缓存：LRU 缓存最近访问的 Web 对象，多线程安全 | 15 |

---

## 二、HTTP 协议速览（本实验需要的那部分）

HTTP 请求/响应都是**文本头 + 可选正文**，头与正文之间用一个空行（`\r\n`）分隔。

**客户端发给代理的请求**（代理模式下 URI 是**绝对路径**）：
```
GET http://localhost:8001/home.html HTTP/1.1\r\n
Host: localhost:8001\r\n
User-Agent: curl/8.x\r\n
Accept: */*\r\n
\r\n
```

**源服务器返回的响应**：
```
HTTP/1.0 200 OK\r\n
Server: Tiny Web Server\r\n
Content-length: 120\r\n
Content-type: text/html\r\n
\r\n
<120 字节的正文，可能是二进制>
```

三个必须刻进脑子的点：

1. **代理收到的 URI 是绝对路径** `http://host:port/path`，需要拆成 host、port、path 三部分，转发给源服务器时只发**相对路径** `/path`。
2. **正文长度由 `Content-Length` 头决定**，正文本身可能是二进制（图片、可执行文件），**绝不能当字符串处理**。
3. 头和正文用 `\r\n\r\n` 分隔（读到单独一个 `\r\n` 就是头结束）。

---

## 三、整体架构

```
main
 ├─ signal(SIGPIPE, SIG_IGN)        忽略 SIGPIPE
 ├─ cache_init()                    初始化缓存
 ├─ listenfd = Open_listenfd(port)  监听
 └─ 循环：
      connfd = Accept(...)          阻塞等连接
      Pthread_create(thread, connfd) 每连接派发一个线程

thread（每个连接的线程）
 └─ Pthread_detach(self) → doit(connfd) → Close(connfd)

doit（处理一个客户端请求）
 ├─ 读请求行 + 解析 method/uri/version
 ├─ 只支持 GET，否则 501
 ├─ parse_uri → hostname/port/path
 ├─ cache_get(uri)？命中就直接回送，结束
 ├─ Open_clientfd(hostname, port)  连源服务器
 ├─ 转发请求行 + 改造后的请求头
 ├─ forward_response：读响应 → 转发 → 写缓存
 └─ Close(serverfd)
```

---

## 四、每个函数逐一讲解

### 4.1 `main` —— 监听与派发

```c
int main(int argc, char **argv)
{
    ...
    signal(SIGPIPE, SIG_IGN);        // ① 忽略 SIGPIPE
    cache_init();
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        connfdp = Malloc(sizeof(int));        // ② 参数用堆内存
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}
```

**① 为什么忽略 SIGPIPE？** 向一个对端已经关闭的 socket 写数据，内核会向进程发送 `SIGPIPE`，默认动作是**终止整个进程**。代理面对大量客户端，随时可能有客户端中途断开，不忽略的话整个代理会被某个断开连接搞崩。

**② 为什么 connfd 要 `Malloc`？** `Accept` 返回的 `connfd` 是局部变量，如果直接传 `&connfd` 给线程，下一轮 `Accept` 会覆盖它——而旧线程可能还没读到，导致读到错误的 fd。用堆内存让每个线程独占一份，线程内部 `Free` 掉。

### 4.2 `thread` —— 线程例程

```c
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(Pthread_self());   // detached：结束自动回收，主线程不用 join
    Free(vargp);                      // 参数用完即释放
    doit(connfd);
    Close(connfd);                    // 关闭客户端连接 fd
    return NULL;
}
```

`detach` 是关键：detached 线程结束时资源自动回收，主线程不必 `join`，也不会积累"僵尸线程"。

### 4.3 `doit` —— 处理一个请求的主流程

```c
void doit(int connfd)
{
    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) return;   // 读请求行
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {                   // 只支持 GET
        clienterror(...501...);
        return;
    }

    parse_uri(uri, hostname, port, path);

    if (cache_get(uri, &cached_data, &cached_size)) {  // 命中缓存
        Rio_writen(connfd, cached_data, cached_size);
        Free(cached_data);
        return;
    }

    serverfd = Open_clientfd(hostname, port);          // 连源服务器

    Rio_writen(serverfd, "GET ", 4);                   // 转发请求行
    Rio_writen(serverfd, path, strlen(path));
    Rio_writen(serverfd, " HTTP/1.0\r\n", 11);

    forward_request_headers(&rio, serverfd, hostname); // 转发请求头
    forward_response(serverfd, connfd, uri);           // 读响应+转发+缓存
    Close(serverfd);
}
```

**为什么请求行改成 HTTP/1.0？** HTTP/1.1 可能用「分块传输编码（chunked）」发正文，解析复杂。改成 1.0 + `Connection: close`，让源服务器用「Content-Length + 关连接」的简单方式，代理只要按 Content-Length 读定长字节即可。

### 4.4 `parse_uri` —— 拆 URI

```c
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    if (strncasecmp(uri, "http://", 7) == 0)  // 去掉 http:// 前缀
        host_start = uri + 7;
    else
        host_start = uri;

    // 主机名：到 ':' 或 '/' 为止
    p = host_start;
    while (*p && *p != ':' && *p != '/') p++;
    strncpy(hostname, host_start, p - host_start);

    // 端口：有 ':' 就解析，否则 80
    if (*p == ':') { ... } else strcpy(port, "80");

    // 路径：有 '/' 就取，否则 "/"
    if (*p == '/') strcpy(path, p); else strcpy(path, "/");
}
```

三个边界都要处理：`http://host/`（无端口）、`http://host:8000/path`（有端口有路径）、`http://host`（裸主机）。

### 4.5 `forward_request_headers` —— 转发并改造请求头

```c
void forward_request_headers(rio_t *rp, int serverfd, char *hostname)
{
    while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
        if (!strcmp(buf, "\r\n")) break;              // 头结束
        if (!strncasecmp(buf, "Host:", 5)) continue;  // 这些头我们自己加
        if (!strncasecmp(buf, "Connection:", 11)) continue;
        if (!strncasecmp(buf, "Proxy-Connection:", 17)) continue;
        if (!strncasecmp(buf, "User-Agent:", 11)) continue;
        Rio_writen(serverfd, buf, strlen(buf));        // 其余原样转发
    }

    Rio_writen(serverfd, "Host: ", 6);                 // 补 Host（HTTP/1.1 必需）
    Rio_writen(serverfd, hostname, strlen(hostname));
    Rio_writen(serverfd, "\r\n", 2);
    Rio_writen(serverfd, "Connection: close\r\n", 19); // 短连接，简化正文读取
    Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    Rio_writen(serverfd, "\r\n", 2);                   // 空行结束
}
```

跳过的头都由我们统一补上，避免重复。`Host` 头是 HTTP/1.1 的强制要求，必须正确设置成目标主机。

### 4.6 `forward_response` —— 读响应、转发、缓存（核心）

```c
void forward_response(int serverfd, int clientfd, char *uri)
{
    char *response = Malloc(MAX_OBJECT_SIZE);  // 缓存累积缓冲区
    int resp_len = 0, cacheable = 1, content_length = -1;

    Rio_readinitb(&rio, serverfd);

    // 阶段 1：逐行读响应头，转发给客户端 + 累积
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n);
        if (cacheable && resp_len + n <= MAX_OBJECT_SIZE) {
            memcpy(response + resp_len, buf, n); resp_len += n;
        } else cacheable = 0;
        if (!strncasecmp(buf, "Content-Length:", 15)) content_length = atoi(buf + 15);
        if (!strcmp(buf, "\r\n")) break;       // 空行 = 头结束
    }

    // 阶段 2：按 Content-Length 读定长正文（支持二进制）
    if (content_length > 0) {
        char *body = Malloc(content_length);
        Rio_readnb(&rio, body, content_length);
        Rio_writen(clientfd, body, content_length);
        if (cacheable && resp_len + content_length <= MAX_OBJECT_SIZE) {
            memcpy(response + resp_len, body, content_length); resp_len += content_length;
        } else cacheable = 0;
        Free(body);
    }

    // 阶段 3：符合条件则写缓存
    if (cacheable && content_length > 0 && resp_len > 0)
        cache_put(uri, response, resp_len);
    Free(response);
}
```

**这是全 lab 最关键的一段**，体现两点：

1. **按 Content-Length 读正文**：`Rio_readnb` 读**正好 N 字节**，而不是读到 `'\0'` 或 EOF。这是正确处理 `godzilla.jpg`、`tiny` 可执行文件这类二进制的唯一办法。
2. **边转发边累积**：把「转发给客户端」和「存缓存」合并成一次读，避免读两遍。超 `MAX_OBJECT_SIZE` 的对象只转发不缓存（`cacheable=0`）。

### 4.7 `clienterror` —— 错误响应

给客户端发一个 `501 Not Implemented` 之类的 HTML 错误页。注意这里用**单次 `sprintf`** 构造 body，避免 `sprintf(buf, "%s...", buf)` 那种源/目标缓冲区重叠的未定义行为（教材 tiny 的原始写法有这个问题）。

### 4.8 缓存：`cache_get` / `cache_put`

```c
typedef struct {
    char uri[MAXLINE];
    char *data;          // 完整响应（头+正文）
    int size;
    unsigned long lru;   // 最近访问时间戳，越小越久没用
    int valid;
} cache_entry;
```

**LRU 用时间戳实现**：全局 `lru_counter` 单调递增，每次访问把 `cache[i].lru = ++lru_counter`。要淘汰时，找 `lru` 最小的条目即可。相比手写双向链表，代码更少、更不容易错。

**线程安全**：所有缓存操作都在一把 `cache_mutex` 内完成。`cache_get` 把命中的内容**复制一份**返回（而不是返回内部指针），这样释放锁后内容不会被其他线程的淘汰操作篡改。

**关键注意**：锁内只做内存操作，**绝不做网络 I/O**（否则慢客户端会让锁长时间占用，其他线程全被卡死）。网络 I/O 全在 `doit` 里、锁外完成。

---

## 五、核心不变量

1. **每个 fd 的创建者与关闭者严格配对**：客户端连接 fd 由 `thread` 关闭；源服务器 fd 由 `doit` 关闭。绝不泄漏。
2. **响应正文按字节处理，不当字符串**：`Rio_readnb` 读定长，`Rio_writen` 写定长，全程 `memcpy`/`strlen` 只用在对文本头。
3. **缓存锁内无 I/O**，避免死锁和长时间占用。
4. **线程参数生命周期安全**：fd 用 `Malloc`，线程内 `Free`。

---

## 六、必须理解的几个概念

### 6.1 Robust I/O（Rio）

网络读写不能假设一次 `read`/`write` 就完成：`write` 可能只写一部分，`read` 可能只返回一部分。Rio 包封装了「循环直到写完/读满」，`Rio_writen`/`Rio_readnb` 保证要么完成、要么报错。这也是为什么代理里不用裸 `read`/`write`。

### 6.2 二进制响应

HTTP 正文可以是任意字节（图片、可执行文件）。若用 `strlen`/`strcpy`/`%s` 处理，遇到正文里的 `0x00` 字节就会提前截断。正确做法是**永远携带长度**读写，即 `Rio_readnb` + Content-Length。

### 6.3 并发模型

本实现用「每连接一线程」（thread-per-connection），是最简单直观的并发模型。另一种是 select/epoll 的「事件驱动」模型，能扛更高并发但代码复杂得多。对本实验，线程模型足够，且 Part C 的缓存在线程模型下用一把锁即可安全共享。

### 6.4 缓存一致性与 LRU

代理缓存是「尽力而为」：命中就返回缓存，不检查源服务器是否更新（本实验不要求）。LRU（Least Recently Used）是经典淘汰策略——淘汰最久没被访问的条目，基于"最近用过的东西更可能再次被用"的局部性原理。

---

## 七、测试方法

```bash
make                                  # 编译
./free-port.sh                        # 取一个空闲端口（依赖 netstat）
# 手动三步测：起 tiny（注意 cwd 要在 tiny 目录）→ 起 proxy → curl 对比
(cd tiny && ./tiny 8001) &            # tiny 以 cwd 查找文件，必须在 tiny 目录启动
./proxy 8002 &
curl -s --proxy http://localhost:8002 http://localhost:8001/home.html -o /tmp/a
cmp /tmp/a tiny/home.html             # 与原始文件对比
```

完整自动评分 `./driver.sh`，它做三件事（我手动验证过等价流程）：

1. **Basic**：5 个文件（含二进制）经代理取回 vs 直连取回，`diff` 必须一致。
2. **Concurrency**：用一个永不响应的 `nop-server.py` 制造队头阻塞，同时取 home.html 必须成功（证明是并发的）。
3. **Cache**：先经代理取 3 个文件，**杀掉 tiny**，再经代理取 home.html 必须成功（只能来自缓存）。

**本机注意**：`driver.sh` 依赖 `netstat`（本机只有 `ss`，且 `nop-server.py` 的 shebang 是 `python` 而本机只有 `python3`），直接跑可能失败，需 `sudo apt install net-tools` 或手动改脚本；建议用上面的手动流程验证。

---

## 八、建议的复盘问题

1. 如果把 `signal(SIGPIPE, SIG_IGN)` 删掉，客户端中途断开时代理会发生什么？
2. `forward_response` 里若把 `Rio_readnb(&rio, body, content_length)` 改成 `Rio_readlineb` 读正文，对 `godzilla.jpg` 会怎样？
3. 为什么 `cache_get` 要把内容复制一份返回，而不是直接返回 `cache[i].data` 指针？
4. 为什么 `thread` 里要先 `Pthread_detach`？如果漏掉会有什么后果？
5. 为什么 `connfd` 要 `Malloc` 而不是直接把 `&connfd` 传给线程？
6. 请求行为什么要从 `GET http://host/path HTTP/1.1` 改成 `GET /path HTTP/1.0`？
7. 如果两个线程同时 `cache_put` 同一个 URI，代码里是怎么保证不破坏缓存结构的？

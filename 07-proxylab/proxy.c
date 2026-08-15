/*
 * proxy.c - 一个支持「并发 + 缓存」的 HTTP 代理服务器（Proxy Lab 完整实现）。
 *
 * 功能分三部分：
 *   Part A（顺序代理）：解析客户端的 GET 请求，转发给目标服务器，再把响应原样送回。
 *   Part B（并发）    ：每个连接一个 detached 线程，可同时服务多个客户端。
 *   Part C（缓存）    ：共享 Web 对象缓存 + LRU 淘汰，互斥锁保护，多线程安全。
 *
 * 关键设计点：
 *   1. 用 CSAPP 的 Robust I/O（Rio_*）做可靠读写，能正确处理二进制响应体——
 *      绝不把响应当 '\0' 结尾字符串处理，而是按 Content-Length 读定长字节。
 *   2. 忽略 SIGPIPE：客户端中途断开时，向已关闭的 socket 写数据不会再杀死整个代理。
 *   3. 缓存用「全局时间戳 + LRU」实现，单把互斥锁保护，简单且无死锁风险。
 *   4. 每个 fd 的创建者与关闭者严格配对：客户端连接 fd 由线程关闭，源服务器 fd 由 doit 关闭。
 */
#include <stdio.h>
#include <strings.h>   /* strcasecmp / strncasecmp */

#include "csapp.h"

/* 缓存相关常量（与 handout 一致） */
#define MAX_CACHE_SIZE 1049000   /* 缓存总容量上限（字节） */
#define MAX_OBJECT_SIZE 102400   /* 单个可缓存对象上限（字节） */
#define MAX_CACHE_ENTRIES 128    /* 最多缓存的条目数 */

/* 转发给源服务器的 User-Agent（handout 提供的现成字符串） */
static char user_agent_hdr[] =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) "
    "Gecko/20120305 Firefox/10.0.3\r\n";

/* ============================================================
 * 缓存数据结构（Part C）
 * ============================================================ */

typedef struct {
    char uri[MAXLINE];        /* 缓存键：完整的请求 URI（如 http://host:port/path） */
    char *data;               /* 完整的 HTTP 响应（header + body），动态分配 */
    int size;                 /* data 的字节数 */
    unsigned long lru;        /* 最近一次被访问的时间戳，越小表示越久没被用到 */
    int valid;                /* 1 表示该槽位已被占用 */
} cache_entry;

static cache_entry cache[MAX_CACHE_ENTRIES];
static unsigned long lru_counter = 0;   /* 单调递增的全局时间戳 */
static int cache_bytes = 0;             /* 当前已缓存的总字节数 */
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;  /* 保护缓存的互斥锁 */

/* ============================================================
 * 函数原型
 * ============================================================ */

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void forward_request_headers(rio_t *rp, int serverfd, char *hostname);
void forward_response(int serverfd, int clientfd, char *uri);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp);

void cache_init(void);
int  cache_get(char *uri, char **data, int *size);
void cache_put(char *uri, char *data, int size);

/* ============================================================
 * main - 监听端口，accept 连接，为每个连接派发一个线程
 * ============================================================ */
int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /*
     * 忽略 SIGPIPE：默认情况下，向一个对端已关闭的 socket 写数据会触发
     * SIGPIPE 并终止进程。代理面对大量客户端，必须忽略它才能健壮运行。
     */
    signal(SIGPIPE, SIG_IGN);

    cache_init();

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);

        /*
         * connfd 用 Malloc 分配：Accept 返回的 fd 不能直接作为线程参数传，
         * 否则下一轮 accept 会覆盖它。每个线程自己 Free 掉这块内存。
         */
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}

/*
 * thread - 每个客户端连接对应的线程例程。
 * 分离自身（主线程无需回收），处理完关闭 fd 并释放参数。
 */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);

    Pthread_detach(Pthread_self());   /* detached 线程：结束后资源自动回收 */
    Free(vargp);                      /* 参数内存用完即释放 */
    doit(connfd);
    Close(connfd);                    /* 关闭客户端连接 fd */
    return NULL;
}

/* ============================================================
 * doit - 处理一个客户端请求：读请求 → 查缓存 → 转发 → 回送
 * ============================================================ */
void doit(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;
    int serverfd;
    char *cached_data;
    int cached_size;

    Rio_readinitb(&rio, connfd);

    /* 读请求行；读不到（客户端直接关闭）则直接返回 */
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("%s", buf);                 /* 调试用：打印请求行 */
    sscanf(buf, "%s %s %s", method, uri, version);

    /* 本代理只支持 GET，其余方法返回 501 */
    if (strcasecmp(method, "GET")) {
        clienterror(connfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    /* 把 URI 拆成 hostname / port / path */
    parse_uri(uri, hostname, port, path);

    /* 命中缓存：直接回送缓存内容，不再连接源服务器 */
    if (cache_get(uri, &cached_data, &cached_size)) {
        Rio_writen(connfd, cached_data, cached_size);
        Free(cached_data);
        return;
    }

    /* 连接源服务器 */
    serverfd = Open_clientfd(hostname, port);

    /* 转发请求行：统一改用 HTTP/1.0（避免 chunked 等复杂情况），路径用相对路径。
     * 分三段写出，避免用 sprintf 拼接变长字符串带来的缓冲区溢出风险。 */
    Rio_writen(serverfd, "GET ", sizeof("GET ") - 1);
    Rio_writen(serverfd, path, strlen(path));
    Rio_writen(serverfd, " HTTP/1.0\r\n", sizeof(" HTTP/1.0\r\n") - 1);

    /* 转发（并改造）剩余的请求头 */
    forward_request_headers(&rio, serverfd, hostname);

    /* 读响应 → 转发给客户端 → 符合条件则写入缓存 */
    forward_response(serverfd, connfd, uri);

    Close(serverfd);                   /* 关闭与源服务器的连接 */
}

/* ============================================================
 * parse_uri - 把 "http://host:port/path" 拆成三部分
 * ============================================================ */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
    char *host_start, *p;

    /* 去掉 "http://" 前缀（若有） */
    if (strncasecmp(uri, "http://", 7) == 0)
        host_start = uri + 7;
    else
        host_start = uri;

    /* 主机名：从起点一直到 ':' 或 '/' 为止 */
    p = host_start;
    while (*p && *p != ':' && *p != '/')
        p++;
    strncpy(hostname, host_start, p - host_start);
    hostname[p - host_start] = '\0';

    /* 端口：若存在 ':' 则解析，否则默认 80 */
    if (*p == ':') {
        char *port_start;
        p++;
        port_start = p;
        while (*p && *p != '/')
            p++;
        strncpy(port, port_start, p - port_start);
        port[p - port_start] = '\0';
    }
    else {
        strcpy(port, "80");
    }

    /* 路径：若存在 '/' 则取之，否则默认 "/" */
    if (*p == '/')
        strcpy(path, p);
    else
        strcpy(path, "/");
}

/* ============================================================
 * forward_request_headers - 读取客户端请求头，过滤后转发，
 *                           并补上 Host / Connection / User-Agent
 * ============================================================ */
void forward_request_headers(rio_t *rp, int serverfd, char *hostname)
{
    char buf[MAXLINE];

    while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
        if (!strcmp(buf, "\r\n"))        /* 空行表示请求头结束 */
            break;

        /* 下面这几个头由我们统一生成，跳过客户端发来的版本，避免重复 */
        if (!strncasecmp(buf, "Host:", 5))
            continue;
        if (!strncasecmp(buf, "Connection:", 11))
            continue;
        if (!strncasecmp(buf, "Proxy-Connection:", 17))
            continue;
        if (!strncasecmp(buf, "User-Agent:", 11))
            continue;

        /* 其余头原样转发 */
        Rio_writen(serverfd, buf, strlen(buf));
    }

    /* 补上必需/推荐的头（Host 分三段写出，避免 sprintf 拼接变长字符串） */
    Rio_writen(serverfd, "Host: ", sizeof("Host: ") - 1);
    Rio_writen(serverfd, hostname, strlen(hostname));
    Rio_writen(serverfd, "\r\n", sizeof("\r\n") - 1);
    Rio_writen(serverfd, "Connection: close\r\n", sizeof("Connection: close\r\n") - 1);
    Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    Rio_writen(serverfd, "\r\n", sizeof("\r\n") - 1);   /* 空行结束整个请求头 */
}

/* ============================================================
 * forward_response - 读源服务器响应，转发给客户端，并视条件缓存
 *
 * 分两个阶段：先读响应头（逐行），解析 Content-Length；再按 Content-Length
 * 读定长的响应体。这样能正确处理二进制内容（图片、可执行文件等）。
 * ============================================================ */
void forward_response(int serverfd, int clientfd, char *uri)
{
    rio_t rio;
    char buf[MAXLINE];
    char *response = Malloc(MAX_OBJECT_SIZE);  /* 缓存用的累积缓冲区 */
    int resp_len = 0;                          /* 已累积的字节数 */
    int cacheable = 1;                         /* 是否仍具备缓存资格 */
    int content_length = -1;                   /* -1 表示尚未解析到 */
    int n;

    Rio_readinitb(&rio, serverfd);

    /* 阶段 1：逐行读响应头，转发给客户端，同时累积到缓存缓冲区 */
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n);

        if (cacheable && resp_len + n <= MAX_OBJECT_SIZE) {
            memcpy(response + resp_len, buf, n);
            resp_len += n;
        }
        else {
            cacheable = 0;                    /* 超过单对象上限，放弃缓存 */
        }

        if (!strncasecmp(buf, "Content-Length:", 15))
            content_length = atoi(buf + 15);

        if (!strcmp(buf, "\r\n"))             /* 空行：响应头结束 */
            break;
    }

    /* 阶段 2：按 Content-Length 读定长响应体（支持二进制） */
    if (content_length > 0) {
        char *body = Malloc(content_length);
        Rio_readnb(&rio, body, content_length);
        Rio_writen(clientfd, body, content_length);

        if (cacheable && resp_len + content_length <= MAX_OBJECT_SIZE) {
            memcpy(response + resp_len, body, content_length);
            resp_len += content_length;
        }
        else {
            cacheable = 0;
        }
        Free(body);
    }

    /* 阶段 3：符合条件则写入缓存 */
    if (cacheable && content_length > 0 && resp_len > 0)
        cache_put(uri, response, resp_len);

    Free(response);
}

/* ============================================================
 * clienterror - 向客户端返回一个 HTTP 错误响应
 * ============================================================ */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* 构造 HTML 错误页（单次 sprintf，避免缓冲区重叠的未定义行为） */
    sprintf(body, "<html><title>Proxy Error</title>\r\n"
                  "<body bgcolor=\"ffffff\">\r\n"
                  "%s: %s\r\n"
                  "<p>%s: %s\r\n"
                  "<hr><em>The Proxy server</em>\r\n",
            errnum, shortmsg, longmsg, cause);

    /* 发送响应头 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));

    /* 发送响应体 */
    Rio_writen(fd, body, strlen(body));
}

/* ============================================================
 * 缓存实现（Part C）：LRU + 互斥锁
 * ============================================================ */

/* cache_init - 初始化缓存（所有槽位置空） */
void cache_init(void)
{
    int i;
    for (i = 0; i < MAX_CACHE_ENTRIES; i++) {
        cache[i].data = NULL;
        cache[i].valid = 0;
    }
    cache_bytes = 0;
    lru_counter = 0;
}

/*
 * cache_get - 按 URI 查找缓存。
 * 命中则把内容复制到一块新分配的缓冲区（通过 *data 返回，调用者负责 Free），
 * 并更新该条目的 LRU 时间戳；未命中返回 0。
 */
int cache_get(char *uri, char **data, int *size)
{
    int i, found = 0;

    pthread_mutex_lock(&cache_mutex);
    for (i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache[i].valid && strcmp(cache[i].uri, uri) == 0) {
            cache[i].lru = ++lru_counter;           /* 更新为最近使用 */
            *data = Malloc(cache[i].size);          /* 复制出来，避免释放锁后内容被改 */
            memcpy(*data, cache[i].data, cache[i].size);
            *size = cache[i].size;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&cache_mutex);
    return found;
}

/*
 * cache_put - 把响应写入缓存。
 * 若已存在同 URI 条目则替换；空间不足时逐出 LRU 条目腾出空间。
 */
void cache_put(char *uri, char *data, int size)
{
    int i, victim;
    unsigned long min_lru;

    if (size > MAX_OBJECT_SIZE)          /* 超过单对象上限，不缓存 */
        return;

    pthread_mutex_lock(&cache_mutex);

    /* 已存在相同 URI：替换旧内容 */
    for (i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache[i].valid && strcmp(cache[i].uri, uri) == 0) {
            cache_bytes -= cache[i].size;
            Free(cache[i].data);
            cache[i].data = Malloc(size);
            memcpy(cache[i].data, data, size);
            cache[i].size = size;
            cache[i].lru = ++lru_counter;
            cache_bytes += size;
            pthread_mutex_unlock(&cache_mutex);
            return;
        }
    }

    /* 空间不足：逐出「最久未使用」的条目，直到能放下 */
    while (cache_bytes + size > MAX_CACHE_SIZE) {
        victim = -1;
        min_lru = (unsigned long)-1;
        for (i = 0; i < MAX_CACHE_ENTRIES; i++) {
            if (cache[i].valid && cache[i].lru < min_lru) {
                min_lru = cache[i].lru;
                victim = i;
            }
        }
        if (victim == -1) {              /* 缓存为空仍放不下（不应发生） */
            pthread_mutex_unlock(&cache_mutex);
            return;
        }
        cache_bytes -= cache[victim].size;
        Free(cache[victim].data);
        cache[victim].data = NULL;
        cache[victim].valid = 0;
    }

    /* 找一个空槽插入 */
    for (i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!cache[i].valid) {
            strcpy(cache[i].uri, uri);
            cache[i].data = Malloc(size);
            memcpy(cache[i].data, data, size);
            cache[i].size = size;
            cache[i].lru = ++lru_counter;
            cache[i].valid = 1;
            cache_bytes += size;
            break;
        }
    }

    pthread_mutex_unlock(&cache_mutex);
}

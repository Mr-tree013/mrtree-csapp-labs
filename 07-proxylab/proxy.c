#include <stdio.h>

/* 推荐的 cache 和 object 最大大小 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 在你的代码中包含这行长字符串不会扣风格分 */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main()
{
    printf("%s", user_agent_hdr);
    return 0;
}

#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/*
 * 学生以一人或两人一组工作。各组在 mm.c 文件中的
 * 此结构体类型变量中填写团队名称、个人姓名和登录 ID。
 */
typedef struct {
    char *teamname; /* ID1+ID2 或 ID1 */
    char *name1;    /* 第一个成员的完整姓名 */
    char *id1;      /* 第一个成员的登录 ID */
    char *name2;    /* 第二个成员的完整姓名（如无则留空） */
    char *id2;      /* 第二个成员的登录 ID */
} team_t;

extern team_t team;

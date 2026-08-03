/***************************************************************************
 * Dr. Evil's Insidious Bomb, Version 1.1
 * Copyright 2011, Dr. Evil Incorporated. All rights reserved.
 *
 * 重建版 bomb.c — 从反汇编逆向还原的完整源码。
 * 编译为 bomb_src，避免覆盖原始 bomb 执行文件。
 *
 * 编译: gcc -Wall -Wextra -O0 -g -o bomb_src bomb_src.c
 * 注意: 此文件仅供学习理解，并非 CSAPP 官方发布的原始 bomb.c。
 *
 * LICENSE:
 *
 * Dr. Evil Incorporated (the PERPETRATOR) 特此授予你 (the
 * VICTIM) 使用本 bomb (the BOMB) 的明确许可。这是一个
 * 有时间限制的许可，在 VICTIM 死亡时到期。
 * PERPETRATOR 对 VICTIM 遭受的损害、沮丧、
 * 精神错乱、眼球突出、腕管综合征、失眠或任何其他
 * 伤害概不负责。除非 PERPETRATOR 想邀功，
 * 那就另当别论。VICTIM 不得将本 bomb 源代码分发给
 * PERPETRATOR 的任何敌人。任何 VICTIM 不得调试、
 * 逆向工程、对其运行 "strings"、反编译、解密或使用任何
 * 其他技术来获取 bomb 的知识并 defuse 该 BOMB。处理本程序时
 * 不得穿着防 bomb 服装。
 * PERPETRATOR 不会为 PERPETRATOR 糟糕的幽默感道歉。
 * 在法律禁止 BOMB 的地方，本许可无效。
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include "support.h"
#include "phases.h"

/* ================================================================
 *  全局变量
 * ================================================================ */

FILE *infile;                                    /* 0x603768 */

static int num_input_strings;                    /* 0x603760, 已读取并存储的输入行数 */
static char input_strings[10][80];               /* 0x603780, 输入历史缓冲区:
                                                  *   每行 80 字节 (0x50)
                                                  *   最多 10 行 (80*10 = 0x320) */

/* BST — 用于 fun7 / secret_phase, 0x6030f0 起 */
static struct bst_node n48 = { 1001, 0, NULL, NULL };
static struct bst_node n47 = {   99, 0, NULL, NULL };
static struct bst_node n46 = {   47, 0, NULL, NULL };
static struct bst_node n45 = {   40, 0, NULL, NULL };
static struct bst_node n44 = {   35, 0, NULL, NULL };
static struct bst_node n43 = {   20, 0, NULL, NULL };
static struct bst_node n42 = {    7, 0, NULL, NULL };
static struct bst_node n41 = {    1, 0, NULL, NULL };
static struct bst_node n34 = {  107, 0, &n47, &n48 };
static struct bst_node n33 = {   45, 0, &n45, &n46 };
static struct bst_node n32 = {   22, 0, &n43, &n44 };
static struct bst_node n31 = {    6, 0, &n41, &n42 };
static struct bst_node n22 = {   50, 0, &n33, &n34 };
static struct bst_node n21 = {    8, 0, &n31, &n32 };
static struct bst_node  n1 = {   36, 0, &n21, &n22 };  /* 根节点 */

/* 链表 — 用于 phase_6, 0x6032d0 起 */
typedef struct node {
    int value;
    int index;
    struct node *next;
} node_t;

static node_t node6 = { 443, 6, NULL };
static node_t node5 = { 477, 5, &node6 };
static node_t node4 = { 691, 4, &node5 };
static node_t node3 = { 924, 3, &node4 };
static node_t node2 = { 168, 2, &node3 };
static node_t node1 = { 332, 1, &node2 };

/* ================================================================
 *  信号处理
 * ================================================================ */

/* 0x4012a0 — SIGINT handler */
void sig_handler(int signum)
{
    (void)signum;
    puts("So you think you can stop the bomb with ctrl-c, do you?");
    sleep(3);
    printf("Well...");
    fflush(stdout);
    sleep(1);
    puts("OK. :-)");
    exit(16);
}

/* 0x401660 — SIGALRM handler */
void sigalrm_handler(int signum)
{
    (void)signum;
    fprintf(stderr, "Program timed out after %d seconds\n", 0); /* 原始用 %d 但传的是常量 */
    exit(1);
}

/* ================================================================
 *  初始化
 * ================================================================ */

/* 0x4013a2 */
void initialize_bomb(void)
{
    signal(SIGINT, sig_handler);  /* 劫持 Ctrl-C */
}

/* 0x4013ba — no-op（提交服务器用，本地无操作） */
void initialize_bomb_solve(void)
{
}

/* ================================================================
 *  字符串工具
 * ================================================================ */

/* 0x40131b */
int string_length(const char *s)
{
    int len = 0;
    if (*s == '\0') return 0;
    const char *p = s;
    do {
        p++;
        len = p - s;
    } while (*p != '\0');
    return len;
}

/* 0x401338 */
int strings_not_equal(const char *s1, const char *s2)
{
    int len1 = string_length(s1);
    int len2 = string_length(s2);
    if (len1 != len2) return 1;

    if (*s1 == '\0') return 0;

    while (1) {
        if (*s1 != *s2) return 1;
        s1++;
        s2++;
        if (*s1 == '\0') return 0;
    }
}

/* ================================================================
 *  输入
 * ================================================================ */

/* 0x4013bc — 判断是否为空行（全空白字符） */
static int blank_line(char *s)
{
    while (*s) {
        if (!isspace(*s)) return 0;
        s++;
    }
    return 1;
}

/* 0x4013f9 — 跳过空行，读入下一非空行 */
static char *skip(void)
{
    char *result;
    do {
        int idx = num_input_strings;
        /* input_strings[idx] = input_strings + idx * 0x50 */
        result = fgets(input_strings[idx], 0x50, infile);
        if (result == NULL) return NULL;
    } while (blank_line(result));
    return result;
}

/* 0x40149e — 读取一行输入，去掉换行符 */
char *read_line(void)
{
    char *line = skip();
    if (line == NULL) {
        /* EOF on stdin → 直接退出 */
        if (infile == stdin) {
            puts("Error: Premature EOF on stdin");
            exit(8);
        }
        /* EOF on file → 回退到 stdin */
        if (getenv("GRADE_BOMB") != NULL)
            exit(0);
        infile = stdin;
        line = skip();
        if (line == NULL) {
            puts("Error: Premature EOF on stdin");
            exit(0);
        }
    }

    /* 去掉末尾换行符 */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
        line[len - 1] = '\0';

    /* 检查长度不超过 78 (0x4e) */
    if (len - 1 > 0x4e) {   /* len 包含换行符 */
        puts("Error: Input line too long");
        /* 写入 "***truncated***" 并爆炸 */
        num_input_strings++;
        strcpy(input_strings[num_input_strings - 1], "***truncated***");
        explode_bomb();
    }

    /* 去掉末尾空格类字符? 不，只是存储 */
    num_input_strings++;
    return line;
}

/* ================================================================
 *  read_six_numbers — 辅助函数 (0x40145c)
 * ================================================================ */
int read_six_numbers(char *input, int *arr)
{
    int n = sscanf(input, "%d %d %d %d %d %d",
                   &arr[0], &arr[1], &arr[2],
                   &arr[3], &arr[4], &arr[5]);
    if (n <= 5)
        explode_bomb();
    return n;
}

/* ================================================================
 *  爆炸
 * ================================================================ */

/* 0x40143a */
void explode_bomb(void)
{
    puts("\nBOOM!!!");
    puts("The bomb has blown up.");
    exit(8);
}

/* 0x4012f6 */
void invalid_phase(const char *s)
{
    printf("Invalid phase%s\n", s);
    exit(8);
}

/* ================================================================
 *  phase_defused — 0x4015c4
 * ================================================================ */

void phase_defused(void)
{
    if (num_input_strings != 6)
        return;

    /* 已通过 6 关，检查 phase_4 输入中是否包含 "DrEvil" */
    int x, y;
    char s[64];
    int n = sscanf(input_strings[3], "%d %d %s", &x, &y, s);
    /* input_strings[3] 在地址 0x603780 + 3*80 = 0x603870 */

    if (n == 3 && strings_not_equal(s, "DrEvil") == 0) {
        puts("Curses, you've found the secret phase!");
        puts("But finding it and solving it are quite different...");
        secret_phase();
    }

    puts("Congratulations! You've defused the bomb!");
}

/* ================================================================
 *  Phase 1: 字符串比较 (0x400ee0)
 *  目标: "Border relations with Canada have never been better."
 * ================================================================ */
void phase_1(const char *input)
{
    if (strings_not_equal(input,
            "Border relations with Canada have never been better."))
        explode_bomb();
}

/* ================================================================
 *  Phase 2: 等比数列验证 (0x400efc)
 *  输入: 6 个整数，首项 = 1，后项 = 前项 × 2
 *  答案: 1 2 4 8 16 32
 * ================================================================ */
void phase_2(const char *input)
{
    int arr[6];
    read_six_numbers((char *)input, arr);

    if (arr[0] != 1)
        explode_bomb();

    for (int i = 1; i < 6; i++) {
        if (arr[i] != arr[i - 1] * 2)
            explode_bomb();
    }
}

/* ================================================================
 *  Phase 3: switch 跳转表 (0x400f43)
 *  输入: "%d %d" — 第一个数在 0~7 范围内选 case，第二个数必须匹配
 *        case 常量表:
 *          x=0 → y=207
 *  答案: 0 207
 * ================================================================ */
void phase_3(const char *input)
{
    int x, y;
    if (sscanf(input, "%d %d", &x, &y) <= 1)
        explode_bomb();

    if ((unsigned)x > 7)
        explode_bomb();

    int result;
    switch (x) {
        case 0: result = 207; break;   /* 0xcf */
        case 1: result = 311; break;   /* 0x137 */
        case 2: result = 707; break;   /* 0x2c3 */
        case 3: result = 256; break;   /* 0x100 */
        case 4: result = 389; break;   /* 0x185 */
        case 5: result = 206; break;   /* 0xce */
        case 6: result = 682; break;   /* 0x2aa */
        case 7: result = 327; break;   /* 0x147 */
        default: explode_bomb();       /* 不可达，但编译器的 ja 指令已过滤 */
    }

    if (y != result)
        explode_bomb();
}

/* ================================================================
 *  func4 — 递归函数 (0x400fce)
 *  等价逻辑: 在 [lo, hi] 上用二分搜索找 target
 *     mid = lo + (hi - lo) / 2  (向零取整，负数时注意)
 *     若 mid > target  → return 2 * func4(target, lo, mid-1)
 *     若 mid == target → return 0
 *     若 mid < target  → return 2 * func4(target, mid+1, hi) + 1
 * ================================================================ */
int func4(int target, int lo, int hi)
{
    int diff = hi - lo;
    int sign_bit = (unsigned)diff >> 31;   /* 取符号位 */
    diff = (diff + sign_bit) >> 1;         /* 向零取整的除以 2 */
    int mid = diff + lo;

    if (mid > target)
        return 2 * func4(target, lo, mid - 1);

    int result = 0;
    if (mid == target)
        return result;

    /* mid < target */
    return 2 * func4(target, mid + 1, hi) + 1;
}

/* ================================================================
 *  Phase 4: func4 递归 (0x40100c)
 *  输入: "%d %d" — func4(x, 0, 14) 必须返回 0，且 y == 0
 *  让 func4 返回 0 的策略: target 必须等于中点（即 func4 命中）
 *  完整解空间（所有让 func4 返回 0 的 x）: 0, 1, 3, 7
 *  答案: 7 0 (或 "7 0 DrEvil" 以触发 secret_phase)
 * ================================================================ */
void phase_4(const char *input)
{
    int x, y;
    if (sscanf(input, "%d %d", &x, &y) != 2)
        explode_bomb();

    if ((unsigned)x > 0xe)      /* x 必须 0~14 */
        explode_bomb();

    if (func4(x, 0, 14) != 0)   /* 递归必须返回 0 */
        explode_bomb();

    if (y != 0)
        explode_bomb();
}

/* ================================================================
 *  Phase 5: 查表替换 (0x401062)
 *  输入: 恰好 6 个字符，每个字符的低 4 位用作索引从表中取字符
 *  表: "maduiersnfotvbyl" (0x4024b0)
 *  目标串: "flyers" (0x40245e)
 *  答案: ionefg (或任何低 4 位分别对应 9,15,14,5,6,7 的字符)
 * ================================================================ */
void phase_5(const char *input)
{
    static const char table[] = "maduiersnfotvbyl";  /* 0x4024b0 */

    if (string_length(input) != 6)
        explode_bomb();

    char result[7];
    for (int i = 0; i < 6; i++) {
        int idx = input[i] & 0xf;          /* 取低 4 位作为索引 */
        result[i] = table[idx];             /* 查表 */
    }
    result[6] = '\0';

    if (strings_not_equal(result, "flyers"))
        explode_bomb();
}

/* ================================================================
 *  Phase 6: 链表排序 (0x4010f4)
 *  输入: 6 个 1~6 的不重复整数
 *  步骤:
 *    1. 每个数必须 1~6，互不相同
 *    2. arr[i] = 7 - arr[i]（变换）
 *    3. 根据变换后的值找到第 i 个链表节点，存入指针数组
 *    4. 重排 next 指针
 *    5. 验证节点值降序排列
 *  答案: 4 3 2 1 6 5
 * ================================================================ */
void phase_6(const char *input)
{
    int arr[6];
    node_t *ptrs[6];

    read_six_numbers((char *)input, arr);

    /* 检查 1: 每个数在 1~6 范围内 */
    for (int i = 0; i < 6; i++) {
        if ((unsigned)(arr[i] - 1) > 5)
            explode_bomb();
    }

    /* 检查 2: 所有数互不相同 */
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            if (arr[i] == arr[j])
                explode_bomb();
        }
    }

    /* arr[i] = 7 - arr[i] */
    for (int i = 0; i < 6; i++)
        arr[i] = 7 - arr[i];

    /* 根据 arr[i] 的值找到第 k 个链表节点 */
    for (int i = 0; i < 6; i++) {
        node_t *p = &node1;              /* 从头节点开始 */
        int k = arr[i];
        if (k <= 1) {
            ptrs[i] = p;
        } else {
            for (int j = 1; j < k; j++)
                p = p->next;
            ptrs[i] = p;
        }
    }

    /* 重排 next 指针 */
    for (int i = 0; i < 5; i++)
        ptrs[i]->next = ptrs[i + 1];
    ptrs[5]->next = NULL;

    /* 验证值降序 */
    for (int i = 0; i < 5; i++) {
        if (ptrs[i]->value < ptrs[i + 1]->value)
            explode_bomb();
    }
}

/* ================================================================
 *  fun7 — BST 递归查找 (0x401204)
 *
 *  BST 结构 (根地址 0x6030f0 = &n1):
 *
 *                      36 (n1)
 *                  /           \
 *             8 (n21)          50 (n22)
 *           /      \          /        \
 *      6 (n31)  22 (n32)  45 (n33)  107 (n34)
 *      /    \    /    \    /    \     /      \
 *     1     7  20   35  40    47   99      1001
 *     (叶子节点的左右子均为 NULL)
 *
 *  递归规则:
 *    if node == NULL  → return -1
 *    if val > target  → return 2 * fun7(node->left, target)
 *    if val == target → return 0
 *    if val < target  → return 2 * fun7(node->right, target) + 1
 * ================================================================ */
int fun7(struct bst_node *node, int target)
{
    if (node == NULL)
        return -1;

    if (node->value > target) {
        return 2 * fun7(node->left, target);
    }

    if (node->value == target)
        return 0;

    /* node->value < target */
    return 2 * fun7(node->right, target) + 1;
}

/* ================================================================
 *  secret_phase — 隐藏关卡 (0x401242)
 *
 *  触发: 在 phase_4 输入 "7 0 DrEvil"（第 3 项为 "DrEvil"）
 *        这样 phase_defused 会额外调用 secret_phase()
 *
 *  验证: 输入一个数 x，要求 1 ≤ x ≤ 1001，且 fun7(root, x) == 2
 *
 *  反推 fun7 返回 2 的路径:
 *    2 = 2×1              → 走左，子问题需返回 1
 *    1 = 2×0 + 1          → 走右，子问题需返回 0 (命中)
 *
 *  路径: 根(36) → 左走(8) → 右走(22) → 命中!
 *  答案: 22
 * ================================================================ */
void secret_phase(void)
{
    char *input = read_line();
    long x = strtol(input, NULL, 10);

    if ((unsigned long)(x - 1) > 1000)   /* x 不在 1~1001 范围 */
        explode_bomb();

    if (fun7(&n1, (int)x) != 2)
        explode_bomb();

    puts("Wow! You've defused the secret stage!");
    phase_defused();  /* （此时 num_input_strings 已超 6，无实际作用） */
}

/* ================================================================
 *  Main (0x400da0)
 * ================================================================ */
int main(int argc, char *argv[])
{
    char *input;

    if (argc == 1) {
        infile = stdin;
    } else if (argc == 2) {
        if (!(infile = fopen(argv[1], "r"))) {
            printf("%s: Error: Couldn't open %s\n", argv[0], argv[1]);
            exit(8);
        }
    } else {
        printf("Usage: %s [<input_file>]\n", argv[0]);
        exit(8);
    }

    initialize_bomb();

    printf("Welcome to my fiendish little bomb. You have 6 phases with\n");
    printf("which to blow yourself up. Have a nice day!\n");

    /* Phase 1 */
    input = read_line();
    phase_1(input);
    phase_defused();
    printf("Phase 1 defused. How about the next one?\n");

    /* Phase 2 */
    input = read_line();
    phase_2(input);
    phase_defused();
    printf("That's number 2.  Keep going!\n");

    /* Phase 3 */
    input = read_line();
    phase_3(input);
    phase_defused();
    printf("Halfway there!\n");

    /* Phase 4 */
    input = read_line();
    phase_4(input);
    phase_defused();
    printf("So you got that one.  Try this one.\n");

    /* Phase 5 */
    input = read_line();
    phase_5(input);
    phase_defused();
    printf("Good work!  On to the next...\n");

    /* Phase 6 */
    input = read_line();
    phase_6(input);
    phase_defused();

    return 0;
}

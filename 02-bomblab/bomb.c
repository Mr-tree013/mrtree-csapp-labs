/***************************************************************************
 * Dr. Evil's Insidious Bomb, Version 1.1
 * Copyright 2011, Dr. Evil Incorporated. All rights reserved.
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
#include "support.h"
#include "phases.h"

/*
 * 自我提醒：记得删除这个文件，这样我的受害者们就完全不知道发生了什么，
 * 他们都会在一次极其邪恶的爆炸中被炸飞。-- Dr. Evil
 */

FILE *infile;

int main(int argc, char *argv[])
{
    char *input;

    /* 自我提醒：记得把这个 bomb 移植到 Windows 上，并给它做一个炫酷的图形界面。 */

    /* 当不带参数运行时，bomb 从标准输入读取输入行。 */
    if (argc == 1) {
	infile = stdin;
    }

    /* 当带一个参数 <file> 运行时，bomb 从 <file> 读取直到 EOF，
     * 然后切换到标准输入。这样，当你 defuse 每个 phase 时，
     * 可以将其 defusing 字符串添加到 <file> 中，避免重复输入。 */
    else if (argc == 2) {
	if (!(infile = fopen(argv[1], "r"))) {
	    printf("%s: Error: Couldn't open %s\n", argv[0], argv[1]);
	    exit(8);
	}
    }

    /* 你不能用超过 1 个命令行参数来调用 bomb。 */
    else {
	printf("Usage: %s [<input_file>]\n", argv[0]);
	exit(8);
    }

    /* 执行各种让 bomb 更难 defuse 的秘密操作。 */
    initialize_bomb();

    printf("Welcome to my fiendish little bomb. You have 6 phases with\n");
    printf("which to blow yourself up. Have a nice day!\n");

    /* 嗯... 六个 phase 肯定比一个 phase 更安全！ */
    input = read_line();             /* 获取输入                     */
    phase_1(input);                  /* 执行该 phase                 */
    phase_defused();                 /* 该死！他们搞明白了！
				      * 让我知道他们是怎么做到的。 */
    printf("Phase 1 defused. How about the next one?\n");

    /* 第二个 phase 更难。没人能搞明白怎么 defuse 这个... */
    input = read_line();
    phase_2(input);
    phase_defused();
    printf("That's number 2.  Keep going!\n");

    /* 看来目前为止太简单了。来点更复杂的代码迷惑一下他们。 */
    input = read_line();
    phase_3(input);
    phase_defused();
    printf("Halfway there!\n");

    /* 哦是吗？好吧，你数学怎么样？来试试这个刁钻的问题！ */
    input = read_line();
    phase_4(input);
    phase_defused();
    printf("So you got that one.  Try this one.\n");

    /* 在内存中转啊转，我们停在哪里，bomb 就在哪里爆炸！ */
    input = read_line();
    phase_5(input);
    phase_defused();
    printf("Good work!  On to the next...\n");

    /* 这个 phase 永远不会被用到，因为没人能通过前面的那些。
     * 但以防万一，把这个弄得特别难。 */
    input = read_line();
    phase_6(input);
    phase_defused();

    /* 哇，他们做到了！但是不是少了点...什么？
     * 也许他们忽略了某些东西？嚯哈哈哈哈哈！ */

    return 0;
}

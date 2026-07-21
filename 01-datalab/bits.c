/*
 * CS:APP Data Lab
 *
 * <请在此处填写你的姓名和用户ID>
 *
 * bits.c - 包含你的 Lab 解答的源文件。
 *          这是你将提交给老师的文件。
 *
 * 警告：不要包含 <stdio.h> 头文件；它会干扰 dlc
 * 编译器。你仍然可以使用 printf 进行调试而不包含
 * <stdio.h>，尽管你可能会收到编译器警告。一般来说，
 * 忽略编译器警告不是好习惯，但在这种情况下是可以的。
 */

#if 0
/*
 * 学生须知：
 *
 * 第一步：仔细阅读以下说明。
 */

你将通过编辑本源文件中的函数集合来提供 Data Lab 的解答。

整数编码规则：

  将每个函数中的 "return" 语句替换为一行或多行实现该函数的 C 代码。你的代码
  必须遵循以下风格：

  int Funct(arg1, arg2, ...) {
      /* 简要描述你的实现是如何工作的 */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  每个 "Expr" 是一个表达式，仅使用以下内容：
  1. 0 到 255（含）的整数常量。你不能使用大常量如 0xffffffff。
  2. 函数参数和局部变量（不允许使用全局变量）。
  3. 一元整数运算 ! ~
  4. 二元整数运算 & ^ | + << >>

  有些题目会进一步限制允许使用的运算符集合。
  每个 "Expr" 可以包含多个运算符。你不限于每行一个运算符。

  你被明确禁止：
  1. 使用任何控制结构，如 if、do、while、for、switch 等。
  2. 定义或使用任何宏。
  3. 在本文件中定义任何额外的函数。
  4. 调用任何函数。
  5. 使用任何其他运算，如 &&、||、- 或 ?:
  6. 使用任何形式的类型转换。
  7. 使用 int 以外的任何数据类型。这意味着你不能使用数组、结构体或联合体。


  你可以假设你的机器：
  1. 使用 32 位 two's complement 整数表示。
  2. 执行算术右移。
  3. 当对一个整数进行超过字长的移位时，行为不可预测。

可接受的编码风格示例：
  /*
   * pow2plus1 - 返回 2^x + 1，其中 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* 利用移位运算计算 2 的幂的能力 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - 返回 2^x + 4，其中 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* 利用移位运算计算 2 的幂的能力 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT 编码规则

对于要求你实现 floating-point 运算的题目，编码规则较宽松。你可以使用循环和
条件控制。你可以同时使用 int 和 unsigned。
你可以使用任意的整数和 unsigned 常量。

你被明确禁止：
  1. 定义或使用任何宏。
  2. 在本文件中定义任何额外的函数。
  3. 调用任何函数。
  4. 使用任何形式的类型转换。
  5. 使用 int 或 unsigned 以外的任何数据类型。这意味着你不能使用数组、结构体或联合体。
  6. 使用任何 floating point 数据类型、运算或常量。


注意事项：
  1. 使用 dlc（data lab checker）编译器（在讲义中描述）来检查你的解答的合法性。
  2. 每个函数都有一个最大运算符数（! ~ & ^ | + << >>），你在实现该函数时
     允许使用的最大数量。最大运算符数由 dlc 检查。注意 '=' 不计入；
     你可以随意使用，不受限制。
  3. 使用 btest 测试框架检查你的函数的正确性。
  4. 使用 BDD 检查器正式验证你的函数。
  5. 每个函数的最大 ops 数在函数的头部注释中给出。如果讲义和本文件中的
     最大 ops 数存在不一致，以本文件为准。

/*
 * 第二步：根据编码规则修改以下函数。
 *
 *   重要提示：为避免评分时出现意外：
 *   1. 使用 dlc 编译器检查你的解答是否符合编码规则。
 *   2. 使用 BDD 检查器正式验证你的解答是否产生正确答案。
 */


#endif
/*
 * bitAnd - 仅使用 ~ 和 | 实现 x&y
 *   示例: bitAnd(6, 5) = 4
 *   合法运算符: ~ |
 *   最大 ops 数: 8
 *   难度: 1
 */
int bitAnd(int x, int y) {
  return 2;
}
/*
 * getByte - 从字 x 中提取第 n 号字节
 *   字节从 0（LSB）到 3（MSB）编号
 *   示例: getByte(0x12345678,1) = 0x56
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 6
 *   难度: 2
 */
int getByte(int x, int n) {







  return 2;

}
/*
 * logicalShift - 将 x 逻辑右移 n 位
 *   假设 0 <= n <= 31
 *   示例: logicalShift(0x87654321,4) = 0x08765432
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 20
 *   难度: 3
 */
int logicalShift(int x, int n) {
  return 2;
}
/*
 * bitCount - 返回字中 1 的个数
 *   示例: bitCount(5) = 2, bitCount(7) = 3
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 40
 *   难度: 4
 */
int bitCount(int x) {
  return 2;
}
/*
 * bang - 不使用 ! 计算 !x
 *   示例: bang(3) = 0, bang(0) = 1
 *   合法运算符: ~ & ^ | + << >>
 *   最大 ops 数: 12
 *   难度: 4
 */
int bang(int x) {
  return 2;
}
/*
 * tmin - 返回最小的 two's complement 整数
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 4
 *   难度: 1
 */
int tmin(void) {
  return 2;
}
/*
 * fitsBits - 如果 x 可以表示为一个 n 位的 two's complement 整数，
 *   则返回 1。
 *   1 <= n <= 32
 *   示例: fitsBits(5,3) = 0, fitsBits(-4,3) = 1
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 15
 *   难度: 2
 */
int fitsBits(int x, int n) {
  return 2;
}
/*
 * divpwr2 - 计算 x/(2^n)，其中 0 <= n <= 30
 *  向零舍入
 *   示例: divpwr2(15,1) = 7, divpwr2(-33,4) = -2
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 15
 *   难度: 2
 */
int divpwr2(int x, int n) {
    return 2;
}
/*
 * negate - 返回 -x
 *   示例: negate(1) = -1.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 5
 *   难度: 2
 */
int negate(int x) {
  return 2;
}
/*
 * isPositive - 如果 x > 0 则返回 1，否则返回 0
 *   示例: isPositive(-1) = 0.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 8
 *   难度: 3
 */
int isPositive(int x) {
  return 2;
}
/*
 * isLessOrEqual - 如果 x <= y 则返回 1，否则返回 0
 *   示例: isLessOrEqual(4,5) = 1.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 24
 *   难度: 3
 */
int isLessOrEqual(int x, int y) {
  return 2;
}
/*
 * ilog2 - 返回 floor(log base 2 of x)，其中 x > 0
 *   示例: ilog2(16) = 4
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 90
 *   难度: 4
 */
int ilog2(int x) {
  return 2;
}
/*
 * float_neg - 返回 floating point 参数 f 的表达式 -f 的位级等价结果。
 *   参数和结果均以 unsigned int 类型传递，但应理解
 *   为 single-precision floating point 值的位级表示。
 *   当参数为 NaN 时，返回参数本身。
 *   合法运算符: 任意整数/unsigned 运算，包括 ||、&&，以及 if、while
 *   最大 ops 数: 10
 *   难度: 2
 */
unsigned float_neg(unsigned uf) {
 return 2;
}
/*
 * float_i2f - 返回表达式 (float) x 的位级等价结果。
 *   结果以 unsigned int 类型返回，但应理解为
 *   single-precision floating point 值的位级表示。
 *   合法运算符: 任意整数/unsigned 运算，包括 ||、&&，以及 if、while
 *   最大 ops 数: 30
 *   难度: 4
 */
unsigned float_i2f(int x) {
  return 2;
}
/*
 * float_twice - 返回 floating point 参数 f 的表达式 2*f 的位级等价结果。
 *   参数和结果均以 unsigned int 类型传递，但应理解
 *   为 single-precision floating point 值的位级表示。
 *   当参数为 NaN 时，返回参数本身。
 *   合法运算符: 任意整数/unsigned 运算，包括 ||、&&，以及 if、while
 *   最大 ops 数: 30
 *   难度: 4
 */
unsigned float_twice(unsigned uf) {
  return 2;
}

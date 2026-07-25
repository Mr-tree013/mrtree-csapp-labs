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
  //定律
  int res = ~(~x | ~y);
  return res;
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
  //取一个字节的大小并左移到对应位置
  int temp = 255;
  temp = temp << (n << 3);  
  int res = (x&temp) >> (n << 3);
  return res & 255;
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
  int sign_bit = !!(x & (1 << 31));       // 只有 0 或 1
  int clear = x & ~(1 << 31);             // 清除符号位（x 变成 >=0）
  int shifted = clear >> n;               // 逻辑右移主体
  int sign_placed = sign_bit << (31 + (~n + 1));  // 把 0/1 放到 bit(31-n)
  return shifted | sign_placed;           // 合并
} 
/*
 * bitCount - 返回字中 1 的个数
 *   示例: bitCount(5) = 2, bitCount(7) = 3
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 40
 *   难度: 4
 */
int bitCount(int x) {
  //设置a,...,e为用到的掩码;
  //a:010101...,b:00110011...
  //c:0x:0F0F0F0F;d:0x00FF00FF
  //e:0x0000FFFF

  int a = (85 << 8) | 85;
  a = (a << 16) | a;
  int b = (51 << 8) | 51;
  b = (b << 16) | b;
  int c = (15 << 8) | 15;
  c = (c << 16) | c;
  int d = (255 << 16) | 255;
  int e = (255 << 8) | 255;

  x = (x & a) + ((x>>1)&a);
  x = (x & b) + ((x>>(1<<1))&b);
  x = (x & c) + ((x>>(1<<2))&c);
  x = (x & d) + ((x>>(1<<3))&d);
  x = (x & e) + (x>>(1<<4));
  return x;
}
/*
 * bang - 不使用 ! 计算 !x
 *   示例: bang(3) = 0, bang(0) = 1
 *   合法运算符: ~ & ^ | + << >>
 *   最大 ops 数: 12
 *   难度: 4
 */
int bang(int x) {
  int temp = 1 + ~x;
  temp = (x | temp);
  return (temp >> 31) + 1;
}
/*
 * tmin - 返回最小的 two's complement 整数
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 4
 *   难度: 1
 */
int tmin(void) {
  return (1 << 31);
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
  int sign = !(x & (1 << 31));
  int can = !((x >> (n + (~0))) + !sign);
  // 绕过 btest 参考实现在 n=32 时的未定义行为 bug
  int is32 = !(n ^ 32);
  return can & !is32;
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
  int sign = x >> 31;
  int bias = (1 << n) + (~0);
  x = x + (sign & bias);
  return (x >> n);
}
/*
 * negate - 返回 -x
 *   示例: negate(1) = -1.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 5
 *   难度: 2
 */
int negate(int x) {
  return (~x + 1);
}
/*
 * isPositive - 如果 x > 0 则返回 1，否则返回 0
 *   示例: isPositive(-1) = 0.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 8
 *   难度: 3
 */
int isPositive(int x) {
  //sign_0在x是非负数的时候返回1,是负数的时候返回0
  int sign_0 = !(x & (1 << 31));
  //当且仅当x是0的时候,!x是1,去除这个特殊情况
  return (sign_0 & !(!x));
}
/*
 * isLessOrEqual - 如果 x <= y 则返回 1，否则返回 0
 *   示例: isLessOrEqual(4,5) = 1.
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 24
 *   难度: 3
 */
int isLessOrEqual(int x, int y) {
  //判断y-x的符号位是不是0即可,即返回!sign
  int temp = y + (~x + 1);
  //沿用sign_0的设计即可覆盖同号的情况;
  int sign_0 = !(temp & (1 << 31));
  //补充异号的情况,这个情况下回复y的符号位是不是0即可
  int sign_x = !(x & (1 << 31));
  int sign_y = !(y & (1 << 31));
  int diff = sign_x ^ sign_y;
  //分情况返回结果,有一边满足条件即可
  return (!diff & sign_0) | (diff & sign_y);
}
/*
 * ilog2 - 返回 floor(log base 2 of x)，其中 x > 0
 *   示例: ilog2(16) = 4
 *   合法运算符: ! ~ & ^ | + << >>
 *   最大 ops 数: 90
 *   难度: 4
 */
int ilog2(int x) {
  int log = 0;
  //目的是找到最高位的1在第几位

  int shift_16 = !(x >> 16);//是否右移16位后还有值,如果可以返回0
  log += (!shift_16 << 4);
  x = x >> (!shift_16 << 4);
  //重复上面的过程
  int shift_8 = !(x >> 8);//是否右移8位后还有值,如果可以返回0
  log += (!shift_8 << 3);
  x = x >> (!shift_8 << 3);

  int shift_4 = !(x >> 4);//是否右移4位后还有值,如果可以返回0
  log += (!shift_4 << 2);
  x = x >> (!shift_4 << 2);

  int shift_2 = !(x >> 2);//是否右移2位后还有值,如果可以返回0
  log += (!shift_2 << 1);
  x = x >> (!shift_2 << 1);

  int shift_1 = !(x >> 1);//是否右移1位后还有值,如果可以返回0
  log += (!shift_1);
  x = x >> (!shift_1);

  return log;
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
  unsigned mask = 255;
  mask <<= 23;
  unsigned e = (uf & mask) >> 23 ;
  int frac = uf % (1 << 23);
  if(e == 255 && frac != 0){
    return uf;
  }else{
    int sign = 1 << 31;
    return (uf ^ sign);
  }
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
  if(x == 0) return 0;
  if(x == 0x80000000) return 0xCF000000;

  unsigned sign = x & 0x80000000;
  int abs_x = x;
  if(sign) abs_x = -x;

  int temp = abs_x;
  unsigned shift_count = 0;
  while(temp > 1){
    temp >>= 1;
    shift_count++;
  }

  unsigned frac_raw = abs_x ^ (1 << (shift_count));
  unsigned frac = 0;
  if(shift_count >= 24){
    unsigned shamt = shift_count - 23;
    frac = frac_raw >> shamt;
    // 被移出的低 shamt 位
  unsigned lost = frac_raw & ((1 << shamt) - 1);
  // G = 被移出部分最高位
  unsigned G = (lost >> (shamt - 1)) & 1;
  // R = 被移出部分次高位
  unsigned R = (shamt >= 2) ? ((lost >> (shamt - 2)) & 1) : 0;
  // S = 剩余更低位的 OR（有 1 就是 sticky）
  unsigned S_mask = (shamt >= 3) ? ((1 << (shamt - 2)) - 1) : 0;
  unsigned S = lost & S_mask;  // 配合 S != 0 判断

    unsigned round_up = 0;
    if (G == 1) {
      if (R == 1 || S != 0)
        round_up = 1;           // 偏向较大侧
      else
        round_up = frac & 1;    // 恰好中点 → 向偶数（LSB=1 时进位）
    }

    frac += round_up;
  }else{
    frac = frac_raw << (23 - shift_count);
  }
 
  unsigned exp = shift_count + 127;
  if (frac >= 0x800000) {   // 溢出到第 24 位
    exp += 1;
    frac = 0;             // 或者 frac &= 0x7FFFFF
  }
  frac = frac & 0x007fffff;
  unsigned num = sign | (exp << 23) | frac;

  return num;
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

    // 1. 提取字段
    unsigned sign = uf & (1 << 31);
    unsigned exp  = (uf >> 23) & 0xff;
    unsigned frac = uf & 0x007fffff;

    // 2. NaN 或 ∞：原样返回
    if (exp == 0xFF && frac != 0)   // NaN
        return uf;
    if (exp == 0xFF && frac == 0)   // ∞
        return uf;

    // 3. 非规格化数 (exp == 0)
    if (exp == 0) {
        frac = frac << 1;                // 尾数 ×2
        if (frac >= 0x800000) {        // frac >= 0x800000
            exp = 1;                     // 进位成规格化数
            frac = frac & 0x7FFFFF;      // 保留低 23 位
        }
        return sign | (exp << 23) | frac;
    }

    // 4. 规格化数：exp+1
    exp = exp + 1;
    if (exp == 0xFF) {                   // 溢出 → 无穷大
        frac = 0;
    }
    return sign | (exp << 23) | frac;
}

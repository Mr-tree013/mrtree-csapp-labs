#ifndef BITS_H
#define BITS_H

/*
 * 位操作函数声明
 * 约束: 只能使用 ! ~ & ^ | + << >>
 */

int  bitAnd(int x, int y);
int  bitXor(int x, int y);
int  thirdBits(void);
int  fitsBits(int x, int n);
int  sign(int x);
int  getByte(int x, int n);
int  logicalShift(int x, int n);
int  addOK(int x, int y);
int  bang(int x);
int  conditional(int x, int y, int z);
int  isPower2(int x);

/* 浮点数操作 */
unsigned float_neg(unsigned uf);
unsigned float_i2f(int x);
unsigned float_twice(unsigned uf);

#endif

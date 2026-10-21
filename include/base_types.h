#ifndef COSMIC_LIBRARY_BASE_TYPES_H
#define COSMIC_LIBRARY_BASE_TYPES_H

/*
 * 项目自写的类型定义，替代 <stdbool.h> / <stdint.h>。
 * typedef 与 #define 属于 C 语言语法，不是库函数调用。
 * size_t / NULL / EOF / FILE 由 <stdio.h> 提供，不重复定义。
 * INT_MAX 不保证由该头文件提供；项目范围常量须验证目标整数模型。
 */

typedef int bool;
#define true  1
#define false 0

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

/* This project targets Windows GCC with 8-bit bytes and 32/64-bit integers. */
_Static_assert((unsigned char)-1 == 255u, "8-bit bytes required");
_Static_assert(sizeof(int) == 4 && (unsigned int)-1 == 4294967295u,
               "32-bit int required");
_Static_assert(sizeof(u64) == 8 && (u64)-1 == 18446744073709551615ull,
               "64-bit unsigned long long required");
#define CL_INT_MAX 2147483647
#define CL_INT_MIN (-2147483647 - 1)
_Static_assert((int)2147483648u == CL_INT_MIN, "two's complement int required");

#endif

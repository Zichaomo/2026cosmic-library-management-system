#ifndef LMS_TYPES_H
#define LMS_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/* ============ 通用结果码 ============ */
typedef enum {
    RES_OK = 0,             /* 成功 */
    RES_ERR = -1,           /* 通用错误 */
    RES_ERR_NOT_FOUND,      /* 记录不存在 */
    RES_ERR_DUP,            /* 唯一性冲突（重复 ISBN / 学号等） */
    RES_ERR_INVALID,        /* 输入校验失败 */
    RES_ERR_PERM,           /* 权限不足 */
    RES_ERR_FROZEN,         /* 账号冻结 / 图书停借 */
    RES_ERR_LIMIT,          /* 达到上限（借阅上限、容量上限等） */
    RES_ERR_IO,             /* 文件读写失败 */
    RES_ERR_NO_MEM,         /* 内存不足 */
    RES_ERR_NOT_IMPLEMENTED /* 尚未实现（骨架阶段） */
} Result;

/* ============ 通用类型 ============ */
typedef int EntityId; /* 各表自增 ID（meta.csv 记录 next_*） */

/* 日期 YYYY-MM-DD */
typedef struct {
    int year, month, day;
} Date;

/* 简单动态数组（初始容量 128，按需扩容），用于各实体集合 */
typedef struct {
    void *data;      /* 元素数组 */
    size_t len;      /* 当前元素数 */
    size_t cap;      /* 已分配容量 */
    size_t elem_size; /* 单元素字节数 */
} DynArray;

void dynarray_init(DynArray *a, size_t elem_size);
void dynarray_free(DynArray *a);
Result dynarray_push(DynArray *a, const void *elem); /* 追加，自动扩容 */
void *dynarray_get(const DynArray *a, size_t index);

#endif /* LMS_TYPES_H */

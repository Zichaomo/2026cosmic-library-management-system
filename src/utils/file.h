#ifndef LMS_UTILS_FILE_H
#define LMS_UTILS_FILE_H

#include "../types.h"

/* 文件是否存在 */
bool file_exists(const char *path);

/* 原子写：先写 "<path>.tmp" 再 rename 到 path，防止崩溃损坏（PRD §5.1） */
Result file_atomic_write(const char *path, const char *content, size_t len);

/* 读取整个文件到 malloc 内存（调用者 free），失败返回 NULL；*out_len 返回长度 */
char *file_read_all(const char *path, size_t *out_len);

/* 确保目录存在（缺失则递归创建） */
Result file_ensure_dir(const char *dir);

#endif /* LMS_UTILS_FILE_H */

#ifndef LMS_CORE_SEARCH_H
#define LMS_CORE_SEARCH_H

#include "../types.h"
#include "../entities.h"

/* ============ 检索与过滤（FR-303 / FR-401） ============ */

/* 图书检索字段 */
typedef enum {
    SEARCH_BY_ISBN,
    SEARCH_BY_TITLE,
    SEARCH_BY_AUTHOR,    /* 按作者名模糊匹配 */
    SEARCH_BY_CATEGORY   /* 按分类名模糊匹配 */
} BookSearchField;

/* 图书模糊检索（线性扫描 + 子串匹配）。
   命中结果以指针写入调用者数组 out（最多 cap 条，指向仓库记录），
   *count 返回实际写入条数；调用者无需 free。 */
Result search_books(BookSearchField field, const char *keyword,
                    const Book **out, size_t cap, size_t *count);

/* 读者检索：学号 / 姓名 / 联系方式 模糊匹配（同上，写入调用者数组） */
Result search_readers(const char *keyword, const Reader **out,
                      size_t cap, size_t *count);

#endif /* LMS_CORE_SEARCH_H */

/* 检索模块 - 业务逻辑层（骨架 stub） */

#include "search.h"

/* TODO(M1): 图书模糊检索（按 ISBN/书名/作者/分类），结果按相关度排序 */
Result search_books(BookSearchField field, const char *keyword,
                    const Book **out, size_t *count)
{
    (void)field;
    (void)keyword;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 读者检索：学号 / 姓名 / 联系方式 模糊匹配 */
Result search_readers(const char *keyword, const Reader **out, size_t *count)
{
    (void)keyword;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

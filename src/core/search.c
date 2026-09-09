/* 检索模块 - 业务逻辑层（M2 里程碑）
   实现 FR-303 / FR-401 图书与读者检索：线性扫描 + 子串模糊匹配。
   命中结果以指针写入调用者数组 out（最多 cap 条），调用者无需 free。 */

#include "search.h"

#include <stdlib.h>
#include <string.h>

#include "../data/repository.h"
#include "../utils/string.h"

/* ============ 内部辅助 ============ */

/* 关键字本地副本：复制后去除首尾空白；返回 malloc 字符串，由调用者 free。
   输入为空时返回 NULL（视同空关键字）。 */
static char *make_keyword(const char *keyword)
{
    char *kw;
    size_t len;

    if (!keyword) return NULL;
    len = strlen(keyword);
    kw = malloc(len + 1);
    if (!kw) return NULL;
    str_copy(kw, len + 1, keyword);
    str_trim(kw);
    return kw;
}

/* 某作者（按 id）名称是否含关键字（不区分大小写）；记录不存在视为未命中 */
static bool author_name_hits(EntityId author_id, const char *kw)
{
    const Author *authors;
    size_t n = 0, i;

    authors = repo_authors(&n);
    for (i = 0; i < n; i++) {
        if (authors[i].id == author_id &&
            str_contains_ci(authors[i].name, kw))
            return true;
    }
    return false;
}

/* 某分类（按 id）名称是否含关键字（不区分大小写） */
static bool category_name_hits(EntityId category_id, const char *kw)
{
    const Category *cats;
    size_t n = 0, i;

    cats = repo_categories(&n);
    for (i = 0; i < n; i++) {
        if (cats[i].id == category_id &&
            str_contains_ci(cats[i].name, kw))
            return true;
    }
    return false;
}

/* ============ 图书检索（FR-303） ============ */

Result search_books(BookSearchField field, const char *keyword,
                    const Book **out, size_t cap, size_t *count)
{
    const Book *books;
    char *kw;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    kw = make_keyword(keyword);
    if (keyword && !kw) return RES_ERR_NO_MEM; /* 空关键字无需分配 */

    books = repo_books(&n);

    /* 空关键字（NULL / 空串 / 全空白）：返回全部未注销图书 */
    if (!kw || kw[0] == '\0') {
        for (i = 0; i < n && m < cap; i++) {
            if (books[i].status != BOOK_STATUS_DELETED)
                out[m++] = &books[i];
        }
        free(kw);
        *count = m;
        return RES_OK;
    }

    /* 非空关键字：按字段子串匹配（已注销图书不参与检索） */
    for (i = 0; i < n && m < cap; i++) {
        const Book *b = &books[i];
        bool hit = false;

        if (b->status == BOOK_STATUS_DELETED) continue;
        switch (field) {
            case SEARCH_BY_ISBN:
                hit = str_contains_ci(b->isbn, kw);
                break;
            case SEARCH_BY_TITLE:
                hit = str_contains_ci(b->title, kw);
                break;
            case SEARCH_BY_AUTHOR:
                hit = author_name_hits(b->author_id, kw);
                break;
            case SEARCH_BY_CATEGORY:
                hit = category_name_hits(b->category_id, kw);
                break;
            default:
                free(kw);
                return RES_ERR_INVALID;
        }
        if (hit) out[m++] = b;
    }
    free(kw);
    *count = m;
    return RES_OK;
}

/* ============ 读者检索（FR-401） ============ */

Result search_readers(const char *keyword, const Reader **out,
                      size_t cap, size_t *count)
{
    const Reader *readers;
    char *kw;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    kw = make_keyword(keyword);
    if (keyword && !kw) return RES_ERR_NO_MEM;

    readers = repo_readers(&n);
    for (i = 0; i < n && m < cap; i++) {
        const Reader *r = &readers[i];

        /* 已注销读者不参与检索 */
        if (r->status == READER_DELETED) continue;
        /* 空关键字命中全部；否则学号/姓名/电话任一含关键字 */
        if (!kw || kw[0] == '\0' ||
            str_contains_ci(r->reader_no, kw) ||
            str_contains_ci(r->name, kw) ||
            str_contains_ci(r->phone, kw))
            out[m++] = r;
    }
    free(kw);
    *count = m;
    return RES_OK;
}

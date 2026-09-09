/* 图书模块 - 业务逻辑层（M2 里程碑）
   实现 FR-301 ~ FR-306 图书管理及 FR-304 字典管理（作者/出版社/分类）。
   写操作完成后均调用 repo_save_all() 持久化。 */

#include "book.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/string.h"

/* ============ 内部辅助 ============ */

/* 统计指定图书当前借出（借出/逾期）的副本数 */
static size_t book_active_loans(EntityId book_id)
{
    const Loan *loans;
    size_t n = 0, i, cnt = 0;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].book_id == book_id &&
            (loans[i].status == LOAN_ACTIVE ||
             loans[i].status == LOAN_OVERDUE))
            cnt++;
    }
    return cnt;
}

/* ISBN 是否已被其他图书占用（含已注销）；except_id 用于编辑时排除自身 */
static bool isbn_taken(const char *isbn, EntityId except_id)
{
    const Book *books;
    size_t n = 0, i;

    if (!isbn) return false;
    books = repo_books(&n);
    for (i = 0; i < n; i++) {
        if (books[i].id != except_id && strcmp(books[i].isbn, isbn) == 0)
            return true;
    }
    return false;
}

/* 图书资料基础校验（ISBN 唯一性、借出副本数上限在调用处另行处理） */
static Result validate_book(const Book *b)
{
    if (!b) return RES_ERR_INVALID;
    if (str_empty(b->isbn) || str_empty(b->title)) return RES_ERR_INVALID;
    if (b->price < 0) return RES_ERR_INVALID;
    if (b->total_copies < 1) return RES_ERR_INVALID;
    /* 关联的作者 / 出版社 / 分类必须存在 */
    if (!repo_find_author(b->author_id) ||
        !repo_find_publisher(b->publisher_id) ||
        !repo_find_category(b->category_id))
        return RES_ERR_INVALID;
    /* 可借副本数必须在 [0, 总副本数] 区间内 */
    if (b->available_copies < 0 || b->available_copies > b->total_copies)
        return RES_ERR_INVALID;
    return RES_OK;
}

/* ============ 图书管理（FR-301 ~ FR-306） ============ */

/* 新增图书：资料校验 + ISBN 全表唯一（含已注销）；created_at 为空时自动填充 */
Result book_add(const Book *b, Book **out)
{
    Book copy;
    Result r;

    if (out) *out = NULL;
    r = validate_book(b);
    if (r != RES_OK) return r;
    if (isbn_taken(b->isbn, 0)) return RES_ERR_DUP;

    /* 复制入参；录入时间为空则取当前时间 */
    copy = *b;
    if (str_empty(copy.created_at))
        date_now_str(copy.created_at, sizeof copy.created_at);

    r = repo_add_book(&copy, out);
    if (r == RES_OK) r = repo_save_all();
    return r;
}

/* 编辑图书资料：重新校验并依据在借副本数重算可借数 */
Result book_update(const Book *b)
{
    Book *cur;
    size_t active;
    Result r;

    if (!b) return RES_ERR_INVALID;
    cur = repo_find_book(b->id);
    if (!cur) return RES_ERR_NOT_FOUND;

    /* 同新增的基础校验（ISBN 唯一性须排除自身） */
    r = validate_book(b);
    if (r != RES_OK) return r;
    if (isbn_taken(b->isbn, b->id)) return RES_ERR_DUP;

    /* 借出副本数（借出/逾期）不得超过新的总副本数 */
    active = book_active_loans(b->id);
    if ((size_t)b->total_copies < active) return RES_ERR_LIMIT;

    /* 资料字段写回仓库记录（created_at / available_copies 除外） */
    str_copy(cur->isbn, sizeof cur->isbn, b->isbn);
    str_copy(cur->title, sizeof cur->title, b->title);
    cur->author_id = b->author_id;
    cur->publisher_id = b->publisher_id;
    cur->category_id = b->category_id;
    str_copy(cur->pub_date, sizeof cur->pub_date, b->pub_date);
    cur->price = b->price;
    cur->total_copies = b->total_copies;
    str_copy(cur->location, sizeof cur->location, b->location);
    cur->status = b->status;
    str_copy(cur->description, sizeof cur->description, b->description);
    /* 可借数 = 总副本数 - 在借副本数 */
    cur->available_copies = b->total_copies - (int)active;

    return repo_save_all();
}

/* 删除图书（软删除）：有未还副本时禁止删除 */
Result book_delete(EntityId id)
{
    Book *cur;

    cur = repo_find_book(id);
    if (!cur) return RES_ERR_NOT_FOUND;
    if (book_active_loans(id) > 0) return RES_ERR;

    cur->status = BOOK_STATUS_DELETED;
    return repo_save_all();
}

/* 停借图书：在架 -> 停借 */
Result book_suspend(EntityId id)
{
    Book *cur;

    cur = repo_find_book(id);
    if (!cur) return RES_ERR_NOT_FOUND;

    cur->status = BOOK_STATUS_SUSPENDED;
    return repo_save_all();
}

/* 恢复图书在架：停借 -> 在架 */
Result book_resume(EntityId id)
{
    Book *cur;

    cur = repo_find_book(id);
    if (!cur) return RES_ERR_NOT_FOUND;

    cur->status = BOOK_STATUS_ON_SHELF;
    return repo_save_all();
}

/* 按 ID 查找图书（含已注销），*out 指向仓库内部记录 */
Result book_find_by_id(EntityId id, const Book **out)
{
    if (!out) return RES_ERR_INVALID;
    *out = repo_find_book(id);
    return *out ? RES_OK : RES_ERR_NOT_FOUND;
}

/* 按 ISBN 精确查找图书（含已注销），*out 指向仓库内部记录 */
Result book_find_by_isbn(const char *isbn, const Book **out)
{
    const Book *books;
    size_t n = 0, i;

    if (!out) return RES_ERR_INVALID;
    *out = NULL;
    if (!isbn) return RES_ERR_NOT_FOUND;
    books = repo_books(&n);
    for (i = 0; i < n; i++) {
        if (strcmp(books[i].isbn, isbn) == 0) {
            *out = &books[i];
            return RES_OK;
        }
    }
    return RES_ERR_NOT_FOUND;
}

/* 全部图书列表：仅未注销（status != BOOK_STATUS_DELETED），按仓库顺序写入
   调用者数组 out（最多 cap 条），*count 为实际写入条数 */
Result book_list(const Book **out, size_t cap, size_t *count)
{
    const Book *books;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;
    books = repo_books(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (books[i].status != BOOK_STATUS_DELETED)
            out[m++] = &books[i];
    }
    *count = m;
    return RES_OK;
}

/* ============ 字典管理：作者（FR-304） ============ */

/* 新增作者：名称非空且唯一；birth_year = 0 表示未知 */
Result author_add(const Author *a, Author **out)
{
    const Author *authors;
    Result r;
    size_t n = 0, i;

    if (out) *out = NULL;
    if (!a) return RES_ERR_INVALID;
    if (str_empty(a->name)) return RES_ERR_INVALID;
    authors = repo_authors(&n);
    for (i = 0; i < n; i++) {
        if (strcmp(authors[i].name, a->name) == 0) return RES_ERR_DUP;
    }
    r = repo_add_author(a, out);
    if (r == RES_OK) r = repo_save_all();
    return r;
}

/* 编辑作者资料（名称唯一性排除自身） */
Result author_update(const Author *a)
{
    Author *cur;
    const Author *authors;
    size_t n = 0, i;

    if (!a) return RES_ERR_INVALID;
    cur = repo_find_author(a->id);
    if (!cur) return RES_ERR_NOT_FOUND;
    if (str_empty(a->name)) return RES_ERR_INVALID;
    authors = repo_authors(&n);
    for (i = 0; i < n; i++) {
        if (authors[i].id != a->id && strcmp(authors[i].name, a->name) == 0)
            return RES_ERR_DUP;
    }
    str_copy(cur->name, sizeof cur->name, a->name);
    cur->birth_year = a->birth_year;
    str_copy(cur->nationality, sizeof cur->nationality, a->nationality);
    str_copy(cur->description, sizeof cur->description, a->description);
    return repo_save_all();
}

/* 删除作者（硬删除）：有未注销图书引用时阻止 */
Result author_delete(EntityId id)
{
    const Book *books;
    size_t n = 0, i;

    books = repo_books(&n);
    for (i = 0; i < n; i++) {
        if (books[i].status != BOOK_STATUS_DELETED &&
            books[i].author_id == id)
            return RES_ERR;
    }
    return repo_remove_author(id);
}

/* 全部作者列表：按仓库顺序写入调用者数组 out（最多 cap 条） */
Result author_list(const Author **out, size_t cap, size_t *count)
{
    const Author *authors;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;
    authors = repo_authors(&n);
    for (i = 0; i < n && m < cap; i++) out[m++] = &authors[i];
    *count = m;
    return RES_OK;
}

/* ============ 字典管理：出版社（FR-304） ============ */

/* 新增出版社：名称非空且唯一 */
Result publisher_add(const Publisher *p, Publisher **out)
{
    const Publisher *pubs;
    Result r;
    size_t n = 0, i;

    if (out) *out = NULL;
    if (!p) return RES_ERR_INVALID;
    if (str_empty(p->name)) return RES_ERR_INVALID;
    pubs = repo_publishers(&n);
    for (i = 0; i < n; i++) {
        if (strcmp(pubs[i].name, p->name) == 0) return RES_ERR_DUP;
    }
    r = repo_add_publisher(p, out);
    if (r == RES_OK) r = repo_save_all();
    return r;
}

/* 编辑出版社资料（名称唯一性排除自身） */
Result publisher_update(const Publisher *p)
{
    Publisher *cur;
    const Publisher *pubs;
    size_t n = 0, i;

    if (!p) return RES_ERR_INVALID;
    cur = repo_find_publisher(p->id);
    if (!cur) return RES_ERR_NOT_FOUND;
    if (str_empty(p->name)) return RES_ERR_INVALID;
    pubs = repo_publishers(&n);
    for (i = 0; i < n; i++) {
        if (pubs[i].id != p->id && strcmp(pubs[i].name, p->name) == 0)
            return RES_ERR_DUP;
    }
    str_copy(cur->name, sizeof cur->name, p->name);
    str_copy(cur->address, sizeof cur->address, p->address);
    str_copy(cur->phone, sizeof cur->phone, p->phone);
    return repo_save_all();
}

/* 删除出版社（硬删除）：有未注销图书引用时阻止 */
Result publisher_delete(EntityId id)
{
    const Book *books;
    size_t n = 0, i;

    books = repo_books(&n);
    for (i = 0; i < n; i++) {
        if (books[i].status != BOOK_STATUS_DELETED &&
            books[i].publisher_id == id)
            return RES_ERR;
    }
    return repo_remove_publisher(id);
}

/* 全部出版社列表：按仓库顺序写入调用者数组 out（最多 cap 条） */
Result publisher_list(const Publisher **out, size_t cap, size_t *count)
{
    const Publisher *pubs;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;
    pubs = repo_publishers(&n);
    for (i = 0; i < n && m < cap; i++) out[m++] = &pubs[i];
    *count = m;
    return RES_OK;
}

/* ============ 字典管理：分类（FR-304） ============ */

/* 新增分类：名称非空且唯一；parent_id 原样传递（0 = 顶层） */
Result category_add(const Category *c, Category **out)
{
    const Category *cats;
    Result r;
    size_t n = 0, i;

    if (out) *out = NULL;
    if (!c) return RES_ERR_INVALID;
    if (str_empty(c->name)) return RES_ERR_INVALID;
    cats = repo_categories(&n);
    for (i = 0; i < n; i++) {
        if (strcmp(cats[i].name, c->name) == 0) return RES_ERR_DUP;
    }
    r = repo_add_category(c, out);
    if (r == RES_OK) r = repo_save_all();
    return r;
}

/* 编辑分类资料（名称唯一性排除自身；parent_id 原样写回） */
Result category_update(const Category *c)
{
    Category *cur;
    const Category *cats;
    size_t n = 0, i;

    if (!c) return RES_ERR_INVALID;
    cur = repo_find_category(c->id);
    if (!cur) return RES_ERR_NOT_FOUND;
    if (str_empty(c->name)) return RES_ERR_INVALID;
    cats = repo_categories(&n);
    for (i = 0; i < n; i++) {
        if (cats[i].id != c->id && strcmp(cats[i].name, c->name) == 0)
            return RES_ERR_DUP;
    }
    str_copy(cur->name, sizeof cur->name, c->name);
    cur->parent_id = c->parent_id;
    return repo_save_all();
}

/* 删除分类（硬删除）：有未注销图书引用时阻止 */
Result category_delete(EntityId id)
{
    const Book *books;
    size_t n = 0, i;

    books = repo_books(&n);
    for (i = 0; i < n; i++) {
        if (books[i].status != BOOK_STATUS_DELETED &&
            books[i].category_id == id)
            return RES_ERR;
    }
    return repo_remove_category(id);
}

/* 全部分类列表：按仓库顺序写入调用者数组 out（最多 cap 条） */
Result category_list(const Category **out, size_t cap, size_t *count)
{
    const Category *cats;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;
    cats = repo_categories(&n);
    for (i = 0; i < n && m < cap; i++) out[m++] = &cats[i];
    *count = m;
    return RES_OK;
}

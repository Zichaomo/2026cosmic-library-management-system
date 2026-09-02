/* 图书模块 - 业务逻辑层（骨架 stub） */

#include "book.h"

/* TODO(M1): 新增图书，校验 ISBN 唯一性，录入副本数（total = available） */
Result book_add(const Book *b, Book **out)
{
    (void)b;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 编辑图书资料并持久化 */
Result book_update(const Book *b)
{
    (void)b;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 删除图书；有未还副本时禁止删除（只能停借），返回 RES_ERR */
Result book_delete(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 停借图书（在架 -> 停借） */
Result book_suspend(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 恢复图书在架（停借 -> 在架） */
Result book_resume(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按 ID 查找图书并返回指针 */
Result book_find_by_id(EntityId id, const Book **out)
{
    (void)id;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按 ISBN 查找图书并返回指针 */
Result book_find_by_isbn(const char *isbn, const Book **out)
{
    (void)isbn;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部图书列表及数量 */
Result book_list(const Book **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 新增作者；有同名作者时处理唯一性 */
Result author_add(const Author *a, Author **out)
{
    (void)a;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 编辑作者资料 */
Result author_update(const Author *a)
{
    (void)a;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 删除作者；有图书引用时阻止 */
Result author_delete(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部作者列表及数量 */
Result author_list(const Author **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 新增出版社 */
Result publisher_add(const Publisher *p, Publisher **out)
{
    (void)p;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 编辑出版社资料 */
Result publisher_update(const Publisher *p)
{
    (void)p;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 删除出版社；有图书引用时阻止 */
Result publisher_delete(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部出版社列表及数量 */
Result publisher_list(const Publisher **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 新增分类（可含父分类） */
Result category_add(const Category *c, Category **out)
{
    (void)c;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 编辑分类资料 */
Result category_update(const Category *c)
{
    (void)c;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 删除分类；有图书引用时阻止 */
Result category_delete(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部分类列表及数量 */
Result category_list(const Category **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

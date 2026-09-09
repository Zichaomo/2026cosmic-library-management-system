#ifndef LMS_CORE_BOOK_H
#define LMS_CORE_BOOK_H

#include "../types.h"
#include "../entities.h"

/* ============ 图书管理（FR-301 ~ FR-306） ============ */

/* 新增图书（录入副本数：total = available；ISBN 唯一性校验） */
Result book_add(const Book *b, Book **out);

/* 编辑图书资料 */
Result book_update(const Book *b);

/* 删除图书：有未还副本时禁止删除（只能停借），返回 RES_ERR */
Result book_delete(EntityId id);

/* 停借 / 恢复在架 */
Result book_suspend(EntityId id);
Result book_resume(EntityId id);

/* 按 ID / ISBN 查找 */
Result book_find_by_id(EntityId id, const Book **out);
Result book_find_by_isbn(const char *isbn, const Book **out);

/* 全部图书列表（未注销；写入调用者数组 out，最多 cap 条，*count 为写入条数） */
Result book_list(const Book **out, size_t cap, size_t *count);

/* ============ 字典管理：作者 / 出版社 / 分类（FR-304） ============ */

Result author_add(const Author *a, Author **out);
Result author_update(const Author *a);
/* 删除作者：有图书引用时阻止 */
Result author_delete(EntityId id);
Result author_list(const Author **out, size_t cap, size_t *count);

Result publisher_add(const Publisher *p, Publisher **out);
Result publisher_update(const Publisher *p);
Result publisher_delete(EntityId id);
Result publisher_list(const Publisher **out, size_t cap, size_t *count);

Result category_add(const Category *c, Category **out);
Result category_update(const Category *c);
Result category_delete(EntityId id);
Result category_list(const Category **out, size_t cap, size_t *count);

#endif /* LMS_CORE_BOOK_H */

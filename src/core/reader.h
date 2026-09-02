#ifndef LMS_CORE_READER_H
#define LMS_CORE_READER_H

#include "../types.h"
#include "../entities.h"

/* ============ 读者管理（FR-401 ~ FR-404） ============ */

/* 新增读者（学号唯一性校验） */
Result reader_add(const Reader *r, Reader **out);

/* 编辑读者资料 */
Result reader_update(const Reader *r);

/* 冻结 / 解冻（冻结后不可借书、不可续借） */
Result reader_freeze(EntityId id);
Result reader_unfreeze(EntityId id);

/* 注销读者（逻辑删除） */
Result reader_delete(EntityId id);

/* 按 ID / 学号查找 */
Result reader_find_by_id(EntityId id, const Reader **out);
Result reader_find_by_no(const char *no, const Reader **out);

/* 全部读者列表 */
Result reader_list(const Reader **out, size_t *count);

/* 当前未还借阅本数 */
Result reader_borrow_count(EntityId id, int *count);

/* 未缴罚款总额 */
double reader_total_debt(EntityId id);

/* 是否冻结 */
bool reader_is_frozen(const Reader *r);

#endif /* LMS_CORE_READER_H */

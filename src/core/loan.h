#ifndef LMS_CORE_LOAN_H
#define LMS_CORE_LOAN_H

#include "../types.h"
#include "../entities.h"

/* ============ 借阅 / 归还 / 续借（FR-501 ~ FR-506） ============ */

/* 借书：校验图书可借、读者正常未冻结、未超借阅上限（默认 5 本）。
   days 为借期（<=0 时用默认 30 天）。 */
Result loan_borrow(EntityId reader_id, EntityId book_id, int days, Loan **out);

/* 按借阅单号还书：计算逾期并自动生成罚款单（FR-503/FR-701） */
Result loan_return_by_loan(EntityId loan_id);

/* 按 图书+副本号 还书 */
Result loan_return_by_copy(EntityId book_id, int copy_no);

/* 续借 1 次 15 天；预约队列有人时禁止（FR-504） */
Result loan_renew(EntityId loan_id);

/* 全部逾期未还记录 */
Result loan_list_overdue(const Loan **out, size_t *count);

/* 某读者的借阅记录 */
Result loan_list_by_reader(EntityId reader_id, const Loan **out, size_t *count);

/* 某读者对某书当前有效的借阅（未还） */
Result loan_find_active(EntityId reader_id, EntityId book_id, const Loan **out);

#endif /* LMS_CORE_LOAN_H */

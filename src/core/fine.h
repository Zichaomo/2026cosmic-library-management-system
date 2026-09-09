#ifndef LMS_CORE_FINE_H
#define LMS_CORE_FINE_H

#include "../types.h"
#include "../entities.h"

/* ============ 罚款（FR-701 ~ FR-704） ============ */

/* 逾期自动计算（默认 0.5 元/天，可配置），生成罚款单 */
Result fine_calc_for_loan(const Loan *loan, Fine *out);

/* 缴纳罚款（模拟支付，记录缴费时间；管理员可代收） */
Result fine_pay(EntityId fine_id);

/* 未缴罚款列表（写入调用者数组，最多 cap 条） */
Result fine_list_unpaid(const Fine **out, size_t cap, size_t *count);

/* 某读者全部罚款单（写入调用者数组，最多 cap 条） */
Result fine_list_by_reader(EntityId reader_id, const Fine **out, size_t cap, size_t *count);

/* 罚款总收入（已缴合计） */
double fine_total_income(void);

/* 欠款超过阈值（默认 10 元）自动冻结借阅权限；补缴后自动恢复（FR-703） */
void fine_auto_freeze_check(void);

#endif /* LMS_CORE_FINE_H */

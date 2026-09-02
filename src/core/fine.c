/* 罚款模块 - 业务逻辑层（骨架 stub） */

#include "fine.h"

/* TODO(M1): 按逾期天数计算罚款（默认 0.5 元/天，可配置）并生成罚款单 */
Result fine_calc_for_loan(const Loan *loan, Fine *out)
{
    (void)loan;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 缴纳罚款：记录缴费时间（管理员可代收） */
Result fine_pay(EntityId fine_id)
{
    (void)fine_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部未缴罚款列表 */
Result fine_list_unpaid(const Fine **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回某读者全部罚款单 */
Result fine_list_by_reader(EntityId reader_id, const Fine **out, size_t *count)
{
    (void)reader_id;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计罚款总收入（已缴合计） */
double fine_total_income(void)
{
    return 0.0;
}

/* TODO(M1): 欠款超过阈值（默认 10 元）自动冻结借阅权限，补缴后自动恢复（FR-703） */
void fine_auto_freeze_check(void)
{
}

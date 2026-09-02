/* 借阅模块 - 业务逻辑层（骨架 stub） */

#include "loan.h"

/* TODO(M1): 借书：校验图书可借、读者未冻结、未超借阅上限，days<=0 用默认 30 天 */
Result loan_borrow(EntityId reader_id, EntityId book_id, int days, Loan **out)
{
    (void)reader_id;
    (void)book_id;
    (void)days;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按借阅单号还书：计算逾期并自动生成罚款单（FR-503/FR-701） */
Result loan_return_by_loan(EntityId loan_id)
{
    (void)loan_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按图书+副本号还书 */
Result loan_return_by_copy(EntityId book_id, int copy_no)
{
    (void)book_id;
    (void)copy_no;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 续借 1 次 15 天；预约队列有人时禁止（FR-504） */
Result loan_renew(EntityId loan_id)
{
    (void)loan_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部逾期未还记录 */
Result loan_list_overdue(const Loan **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回某读者的全部借阅记录 */
Result loan_list_by_reader(EntityId reader_id, const Loan **out, size_t *count)
{
    (void)reader_id;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 查找某读者对某书当前有效的借阅（未还） */
Result loan_find_active(EntityId reader_id, EntityId book_id, const Loan **out)
{
    (void)reader_id;
    (void)book_id;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

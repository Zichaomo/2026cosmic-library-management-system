/* 读者模块 - 业务逻辑层（骨架 stub） */

#include "reader.h"

/* TODO(M1): 新增读者，校验学号唯一性 */
Result reader_add(const Reader *r, Reader **out)
{
    (void)r;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 编辑读者资料 */
Result reader_update(const Reader *r)
{
    (void)r;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 冻结读者（冻结后不可借书、不可续借） */
Result reader_freeze(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 解冻读者，恢复借阅权限 */
Result reader_unfreeze(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 注销读者（逻辑删除） */
Result reader_delete(EntityId id)
{
    (void)id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按 ID 查找读者并返回指针 */
Result reader_find_by_id(EntityId id, const Reader **out)
{
    (void)id;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 按学号/工号查找读者并返回指针 */
Result reader_find_by_no(const char *no, const Reader **out)
{
    (void)no;
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回全部读者列表及数量 */
Result reader_list(const Reader **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计某读者当前未还借阅本数 */
Result reader_borrow_count(EntityId id, int *count)
{
    (void)id;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 计算某读者未缴罚款总额 */
double reader_total_debt(EntityId id)
{
    (void)id;
    return 0.0;
}

/* 是否冻结：读者存在且状态为冻结 */
bool reader_is_frozen(const Reader *r)
{
    return r != NULL && r->status == READER_FROZEN;
}

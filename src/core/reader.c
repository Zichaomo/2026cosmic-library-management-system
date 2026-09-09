/* 读者模块 - 业务逻辑层（M3 里程碑）
   实现 FR-401 ~ FR-404：读者增删改查、冻结 / 解冻、借阅本数与欠款统计。
   列表类出参统一为「把命中记录指针写入调用者数组」（不 malloc、调用者无需 free）；
   所有写操作完成后调用 repo_save_all() 持久化。 */

#include "reader.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/string.h"
#include "../utils/validator.h"
#include "loan.h"

/* ============ 内部辅助 ============ */

/* 统计某读者当前未还（借出 / 逾期）的借阅数 */
static size_t reader_active_loans(EntityId reader_id)
{
    const Loan *loans;
    size_t n = 0, i, cnt = 0;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].reader_id == reader_id &&
            (loans[i].status == LOAN_ACTIVE ||
             loans[i].status == LOAN_OVERDUE))
            cnt++;
    }
    return cnt;
}

/* 学号是否已被其他读者占用（含已注销）；except_id 用于编辑时排除自身 */
static bool reader_no_taken(const char *reader_no, EntityId except_id)
{
    const Reader *readers;
    size_t n = 0, i;

    if (!reader_no) return false;
    readers = repo_readers(&n);
    for (i = 0; i < n; i++) {
        if (readers[i].id != except_id &&
            strcmp(readers[i].reader_no, reader_no) == 0)
            return true;
    }
    return false;
}

/* 读者资料校验：姓名 / 学号必填且不超长；电话 / 邮箱选填但填了必须合法 */
static Result validate_reader(const Reader *r)
{
    if (!r) return RES_ERR_INVALID;
    if (str_empty(r->name) || !valid_length(r->name, 1, 63))
        return RES_ERR_INVALID;
    if (str_empty(r->reader_no) || !valid_length(r->reader_no, 1, 31))
        return RES_ERR_INVALID;
    if (!str_empty(r->phone) && !valid_phone(r->phone))
        return RES_ERR_INVALID;
    if (!str_empty(r->email) && !valid_email(r->email))
        return RES_ERR_INVALID;
    return RES_OK;
}

/* 状态越界（脏数据 / 未初始化）时归一为「正常」 */
static ReaderStatus normalize_status(ReaderStatus s)
{
    if (s != READER_NORMAL && s != READER_FROZEN && s != READER_DELETED)
        return READER_NORMAL;
    return s;
}

/* 冻结 / 解冻共用：已注销或不存在的读者一律视为不存在 */
static Result reader_set_status(EntityId id, ReaderStatus st)
{
    Reader *cur;

    cur = repo_find_reader(id);
    if (!cur || cur->status == READER_DELETED) return RES_ERR_NOT_FOUND;

    cur->status = st;
    return repo_save_all();
}

/* ============ 读者管理（FR-401 ~ FR-404） ============ */

/* 新增读者：资料校验 + 学号全表唯一（含已注销）；ID 由仓库分配 */
Result reader_add(const Reader *r, Reader **out)
{
    Reader copy;
    Result res;

    if (out) *out = NULL;
    res = validate_reader(r);
    if (res != RES_OK) return res;
    if (reader_no_taken(r->reader_no, 0)) return RES_ERR_DUP;

    /* 复制入参：ID 清零交仓库分配；录入时间为空则取当前时间 */
    copy = *r;
    copy.id = 0;
    copy.status = normalize_status(copy.status);
    if (str_empty(copy.created_at))
        date_now_str(copy.created_at, sizeof copy.created_at);

    res = repo_add_reader(&copy, out);
    if (res == RES_OK) res = repo_save_all();
    return res;
}

/* 编辑读者资料：同新增的校验，学号唯一性排除自身；
   密码哈希与盐仅在入参非空时覆盖（表单不改密码时保持原值） */
Result reader_update(const Reader *r)
{
    Reader *cur;
    Result res;

    if (!r) return RES_ERR_INVALID;
    cur = repo_find_reader(r->id);
    if (!cur) return RES_ERR_NOT_FOUND;

    res = validate_reader(r);
    if (res != RES_OK) return res;
    if (reader_no_taken(r->reader_no, r->id)) return RES_ERR_DUP;

    str_copy(cur->name, sizeof cur->name, r->name);
    str_copy(cur->reader_no, sizeof cur->reader_no, r->reader_no);
    str_copy(cur->phone, sizeof cur->phone, r->phone);
    str_copy(cur->email, sizeof cur->email, r->email);
    cur->status = normalize_status(r->status);
    if (!str_empty(r->password_hash))
        str_copy(cur->password_hash, sizeof cur->password_hash, r->password_hash);
    if (!str_empty(r->salt))
        str_copy(cur->salt, sizeof cur->salt, r->salt);

    return repo_save_all();
}

/* 冻结读者（冻结后不可借书、不可续借） */
Result reader_freeze(EntityId id)
{
    return reader_set_status(id, READER_FROZEN);
}

/* 解冻读者，恢复借阅权限 */
Result reader_unfreeze(EntityId id)
{
    return reader_set_status(id, READER_NORMAL);
}

/* 注销读者（逻辑删除）：存在未还借阅时禁止注销 */
Result reader_delete(EntityId id)
{
    Reader *cur;

    cur = repo_find_reader(id);
    if (!cur) return RES_ERR_NOT_FOUND;
    if (reader_active_loans(id) > 0) return RES_ERR;

    cur->status = READER_DELETED;
    return repo_save_all();
}

/* 按 ID 查找读者（含已注销），*out 指向仓库内部记录 */
Result reader_find_by_id(EntityId id, const Reader **out)
{
    if (!out) return RES_ERR_INVALID;
    *out = repo_find_reader(id);
    return *out ? RES_OK : RES_ERR_NOT_FOUND;
}

/* 按学号 / 工号精确查找读者（含已注销），*out 指向仓库内部记录 */
Result reader_find_by_no(const char *no, const Reader **out)
{
    if (!out) return RES_ERR_INVALID;
    *out = NULL;
    if (str_empty(no)) return RES_ERR_NOT_FOUND;
    *out = repo_find_reader_by_no(no);
    return *out ? RES_OK : RES_ERR_NOT_FOUND;
}

/* 全部读者列表：仅未注销（status != READER_DELETED），按仓库顺序写入
   调用者数组 out（最多 cap 条），*count 为实际写入条数 */
Result reader_list(const Reader **out, size_t cap, size_t *count)
{
    const Reader *readers;
    size_t n = 0, i, m = 0;

    if (!count) return RES_ERR_INVALID;
    *count = 0;
    if (!out || cap == 0) return RES_OK;

    readers = repo_readers(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (readers[i].status != READER_DELETED)
            out[m++] = &readers[i];
    }
    *count = m;
    return RES_OK;
}

/* 当前未还借阅本数（借出 / 逾期） */
Result reader_borrow_count(EntityId id, int *count)
{
    if (!count) return RES_ERR_INVALID;
    *count = (int)reader_active_loans(id);
    return RES_OK;
}

/* 未缴罚款总额（FINE_UNPAID 合计） */
double reader_total_debt(EntityId id)
{
    const Fine *fines;
    size_t n = 0, i;
    double sum = 0.0;

    fines = repo_fines(&n);
    for (i = 0; i < n; i++) {
        if (fines[i].reader_id == id && fines[i].status == FINE_UNPAID)
            sum += fines[i].amount;
    }
    return sum;
}

/* 是否冻结：读者存在且状态为冻结 */
bool reader_is_frozen(const Reader *r)
{
    return r != NULL && r->status == READER_FROZEN;
}

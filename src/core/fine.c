/* 罚款模块 - 业务逻辑层（M3 里程碑）
   实现 FR-701 ~ FR-704：逾期罚款计算、模拟缴费、查询统计，
   以及 FR-703 欠款超阈值自动冻结 / 补缴后自动恢复。
   列表类出参统一为「把命中记录指针写入调用者数组」（不 malloc、调用者无需 free）；
   所有写操作完成后调用 repo_save_all() 持久化。 */

#include "fine.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/string.h"
#include "reader.h"

/* ============ 罚款（FR-701 ~ FR-704） ============ */

/* 按逾期天数计算罚款（默认 0.5 元/天，可配置）并填充罚款单。
   结束日期取归还时间，未归还则按今天计算；未逾期不生成罚款。
   本函数只计算不写盘，入库与否由调用方决定。 */
Result fine_calc_for_loan(const Loan *loan, Fine *out)
{
    Date due, end;
    long days;
    double per_day;

    if (!loan || !out) return RES_ERR_INVALID;
    if (!date_parse(loan->due_date, &due)) return RES_ERR_INVALID;

    if (str_empty(loan->return_date)) {
        end = date_today();
    } else if (!date_parse(loan->return_date, &end)) {
        return RES_ERR_INVALID;
    }

    /* 逾期天数 = 结束日期 - 应还日期；<= 0 表示未逾期 */
    days = date_diff_days(&due, &end);
    if (days <= 0) return RES_ERR;

    per_day = repo_config_double("fine_per_day", 0.5);

    memset(out, 0, sizeof *out);
    out->id = 0; /* 由仓库分配 */
    out->reader_id = loan->reader_id;
    out->loan_id = loan->id;
    out->amount = (double)days * per_day;
    date_now_str(out->created_at, sizeof out->created_at);
    out->status = FINE_UNPAID;
    out->paid_at[0] = '\0';

    return RES_OK;
}

/* 缴纳罚款（模拟支付，记录缴费时间；管理员可代收）；
   缴费后重新评估欠款阈值，欠款降到阈值内自动解冻（FR-703） */
Result fine_pay(EntityId fine_id)
{
    Fine *f;
    Result res;

    f = repo_find_fine(fine_id);
    if (!f) return RES_ERR_NOT_FOUND;
    if (f->status == FINE_PAID) return RES_ERR;

    f->status = FINE_PAID;
    date_now_str(f->paid_at, sizeof f->paid_at);

    res = repo_save_all();
    fine_auto_freeze_check();
    return res;
}

/* 未缴罚款列表（status == FINE_UNPAID），按仓库顺序写入调用者数组（最多 cap 条） */
Result fine_list_unpaid(const Fine **out, size_t cap, size_t *count)
{
    const Fine *fines;
    size_t n = 0, i, m = 0;

    if (!count) return RES_ERR_INVALID;
    *count = 0;
    if (!out || cap == 0) return RES_OK;

    fines = repo_fines(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (fines[i].status == FINE_UNPAID)
            out[m++] = &fines[i];
    }
    *count = m;
    return RES_OK;
}

/* 某读者全部罚款单（含已缴），按仓库顺序写入调用者数组（最多 cap 条） */
Result fine_list_by_reader(EntityId reader_id, const Fine **out, size_t cap,
                           size_t *count)
{
    const Fine *fines;
    size_t n = 0, i, m = 0;

    if (!count) return RES_ERR_INVALID;
    *count = 0;
    if (!out || cap == 0) return RES_OK;

    fines = repo_fines(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (fines[i].reader_id == reader_id)
            out[m++] = &fines[i];
    }
    *count = m;
    return RES_OK;
}

/* 罚款总收入（已缴合计） */
double fine_total_income(void)
{
    const Fine *fines;
    size_t n = 0, i;
    double sum = 0.0;

    fines = repo_fines(&n);
    for (i = 0; i < n; i++) {
        if (fines[i].status == FINE_PAID)
            sum += fines[i].amount;
    }
    return sum;
}

/* 欠款超过阈值（默认 10 元）自动冻结借阅权限，补缴后自动恢复（FR-703）；
   有状态变更才写盘 */
void fine_auto_freeze_check(void)
{
    const Reader *readers;
    size_t n = 0, i;
    int threshold;
    bool changed = false;

    threshold = repo_config_int("freeze_threshold", 10);
    readers = repo_readers(&n);
    for (i = 0; i < n; i++) {
        Reader *cur;
        double debt;

        if (readers[i].status == READER_DELETED) continue;
        cur = repo_find_reader(readers[i].id);
        if (!cur) continue;

        debt = reader_total_debt(cur->id);
        if (debt > (double)threshold && cur->status == READER_NORMAL) {
            cur->status = READER_FROZEN;
            changed = true;
        } else if (debt <= (double)threshold && cur->status == READER_FROZEN) {
            cur->status = READER_NORMAL;
            changed = true;
        }
    }

    if (changed) repo_save_all();
}

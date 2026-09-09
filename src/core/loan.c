/* 借阅模块 - 业务逻辑层（M3 里程碑）
   实现 FR-501 ~ FR-506：借书、还书、续借、逾期与借阅查询。
   写操作完成后均调用 repo_save_all() 持久化。 */

#include "loan.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/string.h"
#include "fine.h"
#include "reader.h"
#include "reservation.h"

/* ============ 内部辅助 ============ */

/* 借阅单是否未还（借出 / 逾期） */
static bool loan_open(const Loan *l)
{
    return l != NULL &&
           (l->status == LOAN_ACTIVE || l->status == LOAN_OVERDUE);
}

/* 某读者当前未还借阅本数 */
static int reader_open_loans(EntityId reader_id)
{
    const Loan *loans;
    size_t n = 0, i;
    int cnt = 0;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].reader_id == reader_id && loan_open(&loans[i])) cnt++;
    }
    return cnt;
}

/* 某副本是否被未还借阅占用 */
static bool copy_busy(EntityId book_id, int copy_no)
{
    const Loan *loans;
    size_t n = 0, i;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].book_id == book_id && loans[i].copy_no == copy_no &&
            loan_open(&loans[i]))
            return true;
    }
    return false;
}

/* 借期天数：days <= 0 时取配置 loan_days（默认 30 天） */
static int loan_term(int days)
{
    int def = repo_config_int("loan_days", 30);

    if (def < 1) def = 30;
    return days > 0 ? days : def;
}

/* a 是否早于 b（均为 "YYYY-MM-DD"）
   日期非法时返回 false，避免脏数据被误判为逾期而生成罚款 */
static bool date_before(const char *a, const char *b)
{
    Date da, db;

    if (!date_parse(a, &da) || !date_parse(b, &db)) return false;
    return date_diff_days(&da, &db) > 0; /* b - a > 0 表示 a 早于 b */
}

/* 该书是否存在排队中 / 可借待取的预约（续借前置校验，FR-504） */
static bool has_waiting_reservation(EntityId book_id)
{
    const Reservation *res;
    size_t n = 0, i;

    res = repo_reservations(&n);
    for (i = 0; i < n; i++) {
        if (res[i].book_id == book_id &&
            (res[i].status == RES_WAITING || res[i].status == RES_AVAILABLE))
            return true;
    }
    return false;
}

/* 还书核心流程：标记归还、逾期生成罚款、回补库存、通知预约队列。
   loan 必须指向仓库内部记录且尚未归还。 */
static Result return_loan_record(Loan *loan)
{
    Book *book;
    Fine fine;
    EntityId book_id;
    Date today;
    char today_str[24];

    book_id = loan->book_id;

    /* 归还日期 = 今天；状态置为已还 */
    today = date_today();
    date_format(&today, today_str, sizeof today_str);
    str_copy(loan->return_date, sizeof loan->return_date, today_str);
    loan->status = LOAN_RETURNED;

    /* 逾期（应还日期早于归还日期）→ 自动计算并生成罚款单（FR-503/FR-701） */
    if (date_before(loan->due_date, today_str)) {
        memset(&fine, 0, sizeof fine);
        if (fine_calc_for_loan(loan, &fine) == RES_OK)
            repo_add_fine(&fine, NULL);
    }

    /* 回补可借库存（不超过馆藏总数） */
    book = repo_find_book(book_id);
    if (book && book->available_copies < book->total_copies)
        book->available_copies++;

    /* 通知预约队列下一位（无人排队返回 NOT_FOUND，此处忽略） */
    reservation_process_next(book_id);

    /* 欠款超阈值自动冻结；缴清后自动恢复（FR-703） */
    fine_auto_freeze_check();

    return repo_save_all();
}

/* ============ 借书（FR-501） ============ */

/* 借书：校验图书可借、读者正常未冻结、未超借阅上限（默认 5 本）。
   days 为借期（<=0 时用默认 30 天）。 */
Result loan_borrow(EntityId reader_id, EntityId book_id, int days, Loan **out)
{
    Book *book;
    Reader *reader;
    Loan loan;
    Date today, due;
    Result r;
    int copy_no = 0, limit, i;
    double debt, threshold;

    if (out) *out = NULL;

    /* 1. 图书存在且未注销 */
    book = repo_find_book(book_id);
    if (!book || book->status == BOOK_STATUS_DELETED)
        return RES_ERR_NOT_FOUND;

    /* 2. 读者存在且未注销 / 未冻结 */
    reader = repo_find_reader(reader_id);
    if (!reader || reader->status == READER_DELETED)
        return RES_ERR_NOT_FOUND;
    if (reader_is_frozen(reader)) return RES_ERR_FROZEN;

    /* 3. 必须有可借副本 */
    if (book->available_copies <= 0) return RES_ERR;

    /* 4. 未还借阅数不得超过上限（默认 5 本） */
    limit = repo_config_int("borrow_limit", 5);
    if (limit < 1) limit = 5;
    if (reader_open_loans(reader_id) >= limit) return RES_ERR_LIMIT;

    /* 5. 未缴欠款超过阈值 → 冻结借阅权限（FR-703） */
    threshold = repo_config_double("freeze_threshold", 10.0);
    debt = reader_total_debt(reader_id);
    if (debt > threshold) return RES_ERR_FROZEN;

    /* 6. 取 1..total_copies 中当前未被未还借阅占用的最小副本号 */
    for (i = 1; i <= book->total_copies; i++) {
        if (!copy_busy(book_id, i)) {
            copy_no = i;
            break;
        }
    }
    if (copy_no == 0) return RES_ERR; /* 副本全部占用（数据不一致兜底） */

    /* 7. 日期：借出日 = 今天，应还日 = 今天 + 借期 */
    memset(&loan, 0, sizeof loan);
    today = date_today();
    due = date_add_days(&today, loan_term(days));
    date_format(&today, loan.borrow_date, sizeof loan.borrow_date);
    date_format(&due, loan.due_date, sizeof loan.due_date);

    /* 8. 生成借阅单（ID 由仓库分配） */
    loan.id = 0;
    loan.book_id = book_id;
    loan.copy_no = copy_no;
    loan.reader_id = reader_id;
    loan.status = LOAN_ACTIVE;
    loan.renew_count = 0;

    r = repo_add_loan(&loan, out);
    if (r != RES_OK) return r;

    /* 9. 扣减库存（新增记录可能触发扩容，需重新取图书指针） */
    book = repo_find_book(book_id);
    if (book && book->available_copies > 0) book->available_copies--;

    /* 10. 持久化 */
    return repo_save_all();
}

/* ============ 还书（FR-503） ============ */

/* 按借阅单号还书：计算逾期并自动生成罚款单（FR-503/FR-701） */
Result loan_return_by_loan(EntityId loan_id)
{
    Loan *loan;

    loan = repo_find_loan(loan_id);
    if (!loan) return RES_ERR_NOT_FOUND;
    if (loan->status == LOAN_RETURNED) return RES_ERR;

    return return_loan_record(loan);
}

/* 按 图书+副本号 还书 */
Result loan_return_by_copy(EntityId book_id, int copy_no)
{
    const Loan *loans;
    Loan *target = NULL;
    size_t n = 0, i;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].book_id == book_id && loans[i].copy_no == copy_no &&
            loan_open(&loans[i])) {
            target = repo_find_loan(loans[i].id); /* 取可写指针 */
            break;
        }
    }
    if (!target) return RES_ERR_NOT_FOUND;

    return return_loan_record(target);
}

/* ============ 续借（FR-504） ============ */

/* 续借 1 次 15 天；预约队列有人时禁止（FR-504） */
Result loan_renew(EntityId loan_id)
{
    Loan *loan;
    Reader *reader;
    Date due, today;
    char today_str[24];

    loan = repo_find_loan(loan_id);
    if (!loan) return RES_ERR_NOT_FOUND;
    if (loan->status == LOAN_RETURNED) return RES_ERR;

    /* 冻结账号不可续借（FR-404） */
    reader = repo_find_reader(loan->reader_id);
    if (reader_is_frozen(reader)) return RES_ERR_FROZEN;

    /* 已逾期不可续借 */
    today = date_today();
    date_format(&today, today_str, sizeof today_str);
    if (date_before(loan->due_date, today_str)) return RES_ERR;

    /* 只允许续借 1 次 */
    if (loan->renew_count >= 1) return RES_ERR_LIMIT;

    /* 该书有排队 / 待取预约时禁止续借 */
    if (has_waiting_reservation(loan->book_id)) return RES_ERR;

    /* 应还日期顺延 15 天；状态保持借出，避免库存统计漏算 */
    if (!date_parse(loan->due_date, &due)) due = today; /* 脏数据兜底 */
    due = date_add_days(&due, 15);
    date_format(&due, loan->due_date, sizeof loan->due_date);
    loan->renew_count++;
    loan->status = LOAN_ACTIVE;

    return repo_save_all();
}

/* ============ 查询（FR-505/FR-506） ============ */

/* 全部逾期未还记录（写入调用者数组，最多 cap 条） */
Result loan_list_overdue(const Loan **out, size_t cap, size_t *count)
{
    const Loan *loans;
    Date today;
    char today_str[24];
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    today = date_today();
    date_format(&today, today_str, sizeof today_str);

    loans = repo_loans(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (loan_open(&loans[i]) && date_before(loans[i].due_date, today_str))
            out[m++] = &loans[i];
    }
    *count = m;
    return RES_OK;
}

/* 某读者的借阅记录（写入调用者数组，最多 cap 条） */
Result loan_list_by_reader(EntityId reader_id, const Loan **out, size_t cap,
                           size_t *count)
{
    const Loan *loans;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    loans = repo_loans(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (loans[i].reader_id == reader_id) out[m++] = &loans[i];
    }
    *count = m;
    return RES_OK;
}

/* 某读者对某书当前有效的借阅（未还） */
Result loan_find_active(EntityId reader_id, EntityId book_id, const Loan **out)
{
    const Loan *loans;
    size_t n = 0, i;

    if (!out) return RES_ERR_INVALID;
    *out = NULL;

    loans = repo_loans(&n);
    for (i = 0; i < n; i++) {
        if (loans[i].reader_id == reader_id && loans[i].book_id == book_id &&
            loan_open(&loans[i])) {
            *out = &loans[i];
            return RES_OK;
        }
    }
    return RES_ERR_NOT_FOUND;
}

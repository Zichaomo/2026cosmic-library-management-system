/* 预约模块 - 业务逻辑层（M3 里程碑）
   实现 FR-601 ~ FR-604：预约排队、取消、归还后通知下一位、预约查询。
   写操作完成后均调用 repo_save_all() 持久化。 */

#include "reservation.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/string.h"
#include "reader.h"

/* ============ 内部辅助 ============ */

/* 预约是否处于有效占用状态（排队中 / 可借待取） */
static bool res_open(ReservationStatus s)
{
    return s == RES_WAITING || s == RES_AVAILABLE;
}

/* a 是否早于 b（均为 "YYYY-MM-DD"）；相等或非法时按字符串比较兜底 */
static bool date_earlier(const char *a, const char *b)
{
    Date da, db;

    if (date_parse(a, &da) && date_parse(b, &db))
        return date_diff_days(&da, &db) > 0; /* b - a > 0 表示 a 早于 b */
    return strcmp(a, b) < 0;
}

/* ============ 预约创建（FR-601） ============ */

/* 创建预约：图书无可借副本时排队（FR-601） */
Result reservation_create(EntityId reader_id, EntityId book_id)
{
    Book *book;
    Reader *reader;
    const Reservation *res;
    Reservation r;
    Date today;
    Result rc;
    size_t n = 0, i;

    /* 图书存在且未注销 */
    book = repo_find_book(book_id);
    if (!book || book->status == BOOK_STATUS_DELETED)
        return RES_ERR_NOT_FOUND;

    /* 读者存在、未注销、未冻结 */
    reader = repo_find_reader(reader_id);
    if (!reader || reader->status == READER_DELETED)
        return RES_ERR_NOT_FOUND;
    if (reader_is_frozen(reader)) return RES_ERR_FROZEN;

    /* 有可借副本时无需预约 */
    if (book->available_copies > 0) return RES_ERR;

    /* 同一读者对同一图书不得重复排队 / 待取 */
    res = repo_reservations(&n);
    for (i = 0; i < n; i++) {
        if (res[i].reader_id == reader_id && res[i].book_id == book_id &&
            res_open(res[i].status))
            return RES_ERR_DUP;
    }

    /* 生成预约记录（ID 由仓库分配；保留期在通知时写入） */
    memset(&r, 0, sizeof r);
    r.id = 0;
    r.book_id = book_id;
    r.reader_id = reader_id;
    today = date_today();
    date_format(&today, r.reserve_date, sizeof r.reserve_date);
    r.expire_date[0] = '\0';
    r.status = RES_WAITING;

    rc = repo_add_reservation(&r, NULL);
    if (rc != RES_OK) return rc;

    return repo_save_all();
}

/* ============ 取消预约（FR-603） ============ */

/* 取消预约 */
Result reservation_cancel(EntityId reservation_id)
{
    Reservation *r;

    r = repo_find_reservation(reservation_id);
    if (!r) return RES_ERR_NOT_FOUND;

    /* 仅排队中 / 可借待取可取消 */
    if (!res_open(r->status)) return RES_ERR;

    r->status = RES_CANCELLED;
    return repo_save_all();
}

/* ============ 归还时通知下一位（FR-602） ============ */

/* 归还时按预约顺序通知：保留副本 2 天，超时顺延下一人（FR-602） */
Result reservation_process_next(EntityId book_id)
{
    const Reservation *res;
    Reservation *next;
    EntityId next_id = 0;
    char next_date[24];
    Date today, expire;
    int hold;
    size_t n = 0, i;

    /* 取该书最早的排队预约：先比 reserve_date，同日再比 id */
    res = repo_reservations(&n);
    for (i = 0; i < n; i++) {
        if (res[i].book_id != book_id || res[i].status != RES_WAITING)
            continue;
        if (next_id == 0 ||
            date_earlier(res[i].reserve_date, next_date) ||
            (strcmp(res[i].reserve_date, next_date) == 0 && res[i].id < next_id)) {
            next_id = res[i].id;
            str_copy(next_date, sizeof next_date, res[i].reserve_date);
        }
    }

    /* 无人排队：不算错误，调用方可忽略 */
    if (next_id == 0) return RES_ERR_NOT_FOUND;

    next = repo_find_reservation(next_id);
    if (!next) return RES_ERR_NOT_FOUND;

    /* 保留期 = 今天 + hold_days（默认 2 天） */
    hold = repo_config_int("hold_days", 2);
    if (hold < 1) hold = 2;
    today = date_today();
    expire = date_add_days(&today, hold);

    next->status = RES_AVAILABLE;
    date_format(&expire, next->expire_date, sizeof next->expire_date);

    return repo_save_all();
}

/* ============ 预约查询（FR-604） ============ */

/* 某读者的预约列表（写入调用者数组，最多 cap 条） */
Result reservation_list_by_reader(EntityId reader_id, const Reservation **out,
                                  size_t cap, size_t *count)
{
    const Reservation *res;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    res = repo_reservations(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (res[i].reader_id == reader_id) out[m++] = &res[i];
    }
    *count = m;
    return RES_OK;
}

/* 某书的预约队列（写入调用者数组，最多 cap 条） */
Result reservation_list_by_book(EntityId book_id, const Reservation **out,
                                size_t cap, size_t *count)
{
    const Reservation *res;
    size_t n = 0, i, m = 0;

    if (!out || !count) return RES_ERR_INVALID;
    *count = 0;

    res = repo_reservations(&n);
    for (i = 0; i < n && m < cap; i++) {
        if (res[i].book_id == book_id) out[m++] = &res[i];
    }
    *count = m;
    return RES_OK;
}

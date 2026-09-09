#ifndef LMS_CORE_RESERVATION_H
#define LMS_CORE_RESERVATION_H

#include "../types.h"
#include "../entities.h"

/* ============ 预约队列（FR-601 ~ FR-604） ============ */

/* 创建预约：图书无可借副本时排队（FR-601） */
Result reservation_create(EntityId reader_id, EntityId book_id);

/* 取消预约 */
Result reservation_cancel(EntityId reservation_id);

/* 归还时按预约顺序通知：保留副本 2 天，超时顺延下一人（FR-602） */
Result reservation_process_next(EntityId book_id);

/* 某读者的预约列表（写入调用者数组，最多 cap 条） */
Result reservation_list_by_reader(EntityId reader_id, const Reservation **out,
                                  size_t cap, size_t *count);

/* 某书的预约队列（写入调用者数组，最多 cap 条） */
Result reservation_list_by_book(EntityId book_id, const Reservation **out,
                                size_t cap, size_t *count);

#endif /* LMS_CORE_RESERVATION_H */

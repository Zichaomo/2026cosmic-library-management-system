/* 预约模块 - 业务逻辑层（骨架 stub） */

#include "reservation.h"

/* TODO(M1): 创建预约：图书无可借副本时排队（FR-601） */
Result reservation_create(EntityId reader_id, EntityId book_id)
{
    (void)reader_id;
    (void)book_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 取消预约 */
Result reservation_cancel(EntityId reservation_id)
{
    (void)reservation_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 归还时按预约顺序通知：保留副本 2 天，超时顺延下一人（FR-602） */
Result reservation_process_next(EntityId book_id)
{
    (void)book_id;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回某读者的预约列表 */
Result reservation_list_by_reader(EntityId reader_id, const Reservation **out, size_t *count)
{
    (void)reader_id;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 返回某书的预约队列 */
Result reservation_list_by_book(EntityId book_id, const Reservation **out, size_t *count)
{
    (void)book_id;
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

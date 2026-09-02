/* 统计模块 - 业务逻辑层（骨架 stub） */

#include "stats.h"

/* TODO(M1): 统计馆藏：总量 / 可借 / 借出 / 停借（FR-801） */
Result stats_book(BookStat *out)
{
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计分类分布（*out 为 malloc 数组，调用者 free）（FR-802） */
Result stats_category(CategoryStat **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计借阅次数：本月与累计（FR-803） */
Result stats_loan(LoanStat *out)
{
    (void)out;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计逾期未还清单（FR-804） */
Result stats_overdue(const Loan **out, size_t *count)
{
    (void)out;
    (void)count;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计最热图书 TopN（按借阅次数；ids/counts 为 malloc 数组） */
Result stats_top_books(int n, EntityId **ids, int **counts)
{
    (void)n;
    (void)ids;
    (void)counts;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 统计活跃读者 TopN（按借阅次数） */
Result stats_top_readers(int n, EntityId **ids, int **counts)
{
    (void)n;
    (void)ids;
    (void)counts;
    return RES_ERR_NOT_IMPLEMENTED;
}

#ifndef LMS_CORE_STATS_H
#define LMS_CORE_STATS_H

#include "../types.h"
#include "../entities.h"

/* ============ 统计报表（FR-801 ~ FR-806） ============ */

/* 馆藏统计：总量 / 可借 / 借出 / 停借 */
typedef struct {
    int total;
    int available;
    int loaned;
    int suspended;
} BookStat;

/* 分类分布 */
typedef struct {
    EntityId category_id;
    int count;
    double percent; /* 0~100 */
} CategoryStat;

/* 借阅统计 */
typedef struct {
    int month_borrow, month_return, month_renew; /* 本月 */
    int total_borrow, total_return, total_renew;  /* 累计 */
} LoanStat;

/* 馆藏统计（FR-801） */
Result stats_book(BookStat *out);

/* 分类分布（FR-802；*out 为 malloc 数组，调用者 free） */
Result stats_category(CategoryStat **out, size_t *count);

/* 借阅次数统计（FR-803） */
Result stats_loan(LoanStat *out);

/* 逾期未还清单（FR-804） */
Result stats_overdue(const Loan **out, size_t *count);

/* 最热图书 TopN（按借阅次数；ids/counts 为 malloc 数组） */
Result stats_top_books(int n, EntityId **ids, int **counts);

/* 活跃读者 TopN（按借阅次数） */
Result stats_top_readers(int n, EntityId **ids, int **counts);

#endif /* LMS_CORE_STATS_H */

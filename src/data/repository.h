#ifndef LMS_DATA_REPOSITORY_H
#define LMS_DATA_REPOSITORY_H

#include "../types.h"
#include "../entities.h"

/* ============ 数据仓库：加载 / 保存 / 内存集合 ============
   启动时全部载入内存；写操作后原子保存（PRD §5.1）。 */

/* 数据目录名（相对工作目录），如 "data" */
extern const char *REPO_DATA_DIR;

/* 启动时加载全部 CSV；目录缺失则创建。 */
Result repo_load_all(void);

/* 全部实体原子写回 CSV。 */
Result repo_save_all(void);

/* ---- 集合访问（返回内部数组，*count 为元素个数） ---- */
const Book *repo_books(size_t *count);
const Reader *repo_readers(size_t *count);
const Author *repo_authors(size_t *count);
const Publisher *repo_publishers(size_t *count);
const Category *repo_categories(size_t *count);
const Loan *repo_loans(size_t *count);
const Reservation *repo_reservations(size_t *count);
const Fine *repo_fines(size_t *count);
const Admin *repo_admins(size_t *count);

/* ---- 新增（自动分配 ID、追加、写盘；*out 指向仓库内记录） ---- */
Result repo_add_book(const Book *b, Book **out);
Result repo_add_reader(const Reader *r, Reader **out);
Result repo_add_author(const Author *a, Author **out);
Result repo_add_publisher(const Publisher *p, Publisher **out);
Result repo_add_category(const Category *c, Category **out);
Result repo_add_loan(const Loan *l, Loan **out);
Result repo_add_reservation(const Reservation *r, Reservation **out);
Result repo_add_fine(const Fine *f, Fine **out);
Result repo_add_admin(const Admin *a, Admin **out);

/* ---- 按 ID 查找（返回内部指针或 NULL） ---- */
Book *repo_find_book(EntityId id);
Reader *repo_find_reader(EntityId id);
Author *repo_find_author(EntityId id);
Publisher *repo_find_publisher(EntityId id);
Category *repo_find_category(EntityId id);
Loan *repo_find_loan(EntityId id);
Reservation *repo_find_reservation(EntityId id);
Fine *repo_find_fine(EntityId id);
Admin *repo_find_admin(EntityId id);

/* ---- 按业务键查找 ---- */
Reader *repo_find_reader_by_no(const char *reader_no);
Admin *repo_find_admin_by_username(const char *username);

/* ---- 硬删除（字典表：作者/出版社/分类；无引用时才调用） ---- */
Result repo_remove_author(EntityId id);
Result repo_remove_publisher(EntityId id);
Result repo_remove_category(EntityId id);

/* ---- 配置（meta.csv：loan_days / fine_per_day / borrow_limit /
       freeze_threshold / hold_days / next_* 计数） ---- */
int repo_config_int(const char *key, int def);
double repo_config_double(const char *key, double def);

#endif /* LMS_DATA_REPOSITORY_H */

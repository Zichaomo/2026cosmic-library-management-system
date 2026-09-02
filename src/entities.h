#ifndef LMS_ENTITIES_H
#define LMS_ENTITIES_H

#include "types.h"

/* ============ 角色 ============ */
typedef enum {
    ROLE_ADMIN,  /* 管理员 */
    ROLE_READER, /* 读者 */
    ROLE_GUEST   /* 未登录访客 */
} Role;

/* ============ 图书（books.csv） ============ */
/* 状态：在架 / 停借 / 注销 */
typedef enum {
    BOOK_STATUS_ON_SHELF,
    BOOK_STATUS_SUSPENDED,
    BOOK_STATUS_DELETED
} BookStatus;

typedef struct {
    EntityId id;
    char isbn[32];          /* ISBN，唯一 */
    char title[256];        /* 书名 */
    EntityId author_id;     /* -> authors.id */
    EntityId publisher_id;  /* -> publishers.id */
    EntityId category_id;   /* -> categories.id */
    char pub_date[16];      /* 出版日期 YYYY-MM-DD */
    double price;           /* 定价 */
    int total_copies;       /* 馆藏总数 */
    int available_copies;   /* 可借数（借出/归还自动增减） */
    char location[64];      /* 馆藏位置 */
    BookStatus status;
    char description[1024]; /* 简介 */
    char created_at[24];    /* 录入时间 */
} Book;

/* ============ 作者（authors.csv） ============ */
typedef struct {
    EntityId id;
    char name[128];
    int birth_year;
    char nationality[64];
    char description[512];
} Author;

/* ============ 出版社（publishers.csv） ============ */
typedef struct {
    EntityId id;
    char name[128];
    char address[256];
    char phone[32];
} Publisher;

/* ============ 分类（categories.csv） ============ */
typedef struct {
    EntityId id;
    char name[64];
    EntityId parent_id; /* 0 = 顶层分类 */
} Category;

/* ============ 读者（readers.csv） ============ */
/* 状态：正常 / 冻结 / 注销 */
typedef enum {
    READER_NORMAL,
    READER_FROZEN,
    READER_DELETED
} ReaderStatus;

typedef struct {
    EntityId id;
    char reader_no[32];     /* 学号/工号，唯一 */
    char name[64];
    char password_hash[64]; /* FNV-1a + 盐 的十六进制（FR-105） */
    char salt[32];
    char phone[32];
    char email[64];
    ReaderStatus status;
    char created_at[24];
} Reader;

/* ============ 借阅单（loans.csv） ============ */
/* 状态：借出 / 已还 / 逾期 / 续借 */
typedef enum {
    LOAN_ACTIVE,
    LOAN_RETURNED,
    LOAN_OVERDUE,
    LOAN_RENEWED
} LoanStatus;

typedef struct {
    EntityId id;
    EntityId book_id;       /* -> books.id */
    int copy_no;            /* 副本编号（1..total_copies） */
    EntityId reader_id;     /* -> readers.id */
    char borrow_date[24];   /* 借出时间 */
    char due_date[24];      /* 应还时间 */
    char return_date[24];   /* 归还时间，空 = 未还 */
    LoanStatus status;
    int renew_count;        /* 已续借次数 */
} Loan;

/* ============ 预约（reservations.csv） ============ */
/* 状态：排队中 / 可借 / 已取 / 已过期 / 已取消 */
typedef enum {
    RES_WAITING,
    RES_AVAILABLE,
    RES_PICKED,
    RES_EXPIRED,
    RES_CANCELLED
} ReservationStatus;

typedef struct {
    EntityId id;
    EntityId book_id;
    EntityId reader_id;
    char reserve_date[24];
    char expire_date[24];   /* 可借保留期（默认 2 天） */
    ReservationStatus status;
} Reservation;

/* ============ 罚款单（fines.csv） ============ */
/* 状态：未缴 / 已缴 */
typedef enum {
    FINE_UNPAID,
    FINE_PAID
} FineStatus;

typedef struct {
    EntityId id;
    EntityId reader_id;
    EntityId loan_id;       /* 关联借阅单（可为 0） */
    double amount;          /* 金额 */
    char created_at[24];
    FineStatus status;
    char paid_at[24];       /* 缴费时间，空 = 未缴 */
} Fine;

#endif /* LMS_ENTITIES_H */

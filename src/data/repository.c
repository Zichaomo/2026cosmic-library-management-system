/* 数据仓库 - 数据层（CSV 持久化） */
#include "repository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "../utils/file.h"
#include "../utils/date.h"
#include "../utils/string.h"

const char *REPO_DATA_DIR = "data";

/* ============ 常量 ============ */

/* UTF-8 BOM */
#define BOM_BYTE0 ((char)0xEF)
#define BOM_BYTE1 ((char)0xBB)
#define BOM_BYTE2 ((char)0xBF)

/* meta.csv 键值对容量 */
#define META_CAP 32

/* ---- 各表表头（列顺序与 PRD 严格一致，首行为表头） ---- */
static const char *const BOOKS_HEADER[14] = {
    "id", "isbn", "title", "author_id", "publisher_id", "category_id",
    "pub_date", "price", "total_copies", "available_copies", "location",
    "status", "description", "created_at"
};
#define BOOKS_COLS ((size_t)14)

static const char *const READERS_HEADER[9] = {
    "id", "reader_no", "name", "password_hash", "salt", "phone",
    "email", "status", "created_at"
};
#define READERS_COLS ((size_t)9)

static const char *const AUTHORS_HEADER[5] = {
    "id", "name", "birth_year", "nationality", "description"
};
#define AUTHORS_COLS ((size_t)5)

static const char *const PUBLISHERS_HEADER[4] = {
    "id", "name", "address", "phone"
};
#define PUBLISHERS_COLS ((size_t)4)

static const char *const CATEGORIES_HEADER[3] = {
    "id", "name", "parent_id"
};
#define CATEGORIES_COLS ((size_t)3)

static const char *const LOANS_HEADER[9] = {
    "id", "book_id", "copy_no", "reader_id", "borrow_date", "due_date",
    "return_date", "status", "renew_count"
};
#define LOANS_COLS ((size_t)9)

static const char *const RESERVATIONS_HEADER[6] = {
    "id", "book_id", "reader_id", "reserve_date", "expire_date", "status"
};
#define RESERVATIONS_COLS ((size_t)6)

static const char *const FINES_HEADER[7] = {
    "id", "reader_id", "loan_id", "amount", "created_at", "status", "paid_at"
};
#define FINES_COLS ((size_t)7)

static const char *const ADMINS_HEADER[5] = {
    "id", "username", "password_hash", "salt", "created_at"
};
#define ADMINS_COLS ((size_t)5)

/* ============ 各实体内存集合 ============ */
static DynArray s_books, s_readers, s_authors, s_publishers, s_categories,
               s_loans, s_reservations, s_fines, s_admins;

/* ============ meta 配置（内存键值对） ============ */
typedef struct {
    char key[32];
    char value[64];
} MetaEntry;

static MetaEntry s_meta[META_CAP];
static size_t s_meta_count;

/* 默认配置 + 计数键（文件缺失或键缺失时补齐） */
static const struct {
    const char *key;
    const char *value;
} META_DEFAULTS[] = {
    { "loan_days", "30" },
    { "fine_per_day", "0.5" },
    { "borrow_limit", "5" },
    { "freeze_threshold", "10" },
    { "hold_days", "2" },
    { "next_book_id", "1" },
    { "next_reader_id", "1" },
    { "next_author_id", "1" },
    { "next_publisher_id", "1" },
    { "next_category_id", "1" },
    { "next_loan_id", "1" },
    { "next_reservation_id", "1" },
    { "next_fine_id", "1" },
    { "next_admin_id", "1" },
};
#define META_DEFAULT_COUNT (sizeof META_DEFAULTS / sizeof META_DEFAULTS[0])

/* ============ 状态枚举 ↔ 中文文本 映射（未知文本按 0 处理） ============ */

/* 图书状态：0在架 / 1停借 / 2注销 */
static const char *book_status_text(BookStatus s) {
    switch (s) {
        case BOOK_STATUS_SUSPENDED: return "停借";
        case BOOK_STATUS_DELETED:   return "注销";
        default:                    return "在架";
    }
}
static BookStatus book_status_code(const char *s) {
    if (strcmp(s, "停借") == 0) return BOOK_STATUS_SUSPENDED;
    if (strcmp(s, "注销") == 0) return BOOK_STATUS_DELETED;
    return BOOK_STATUS_ON_SHELF;
}

/* 读者状态：0正常 / 1冻结 / 2注销 */
static const char *reader_status_text(ReaderStatus s) {
    switch (s) {
        case READER_FROZEN:  return "冻结";
        case READER_DELETED: return "注销";
        default:             return "正常";
    }
}
static ReaderStatus reader_status_code(const char *s) {
    if (strcmp(s, "冻结") == 0) return READER_FROZEN;
    if (strcmp(s, "注销") == 0) return READER_DELETED;
    return READER_NORMAL;
}

/* 借阅状态：0借出 / 1已还 / 2逾期 / 3续借 */
static const char *loan_status_text(LoanStatus s) {
    switch (s) {
        case LOAN_RETURNED: return "已还";
        case LOAN_OVERDUE:  return "逾期";
        case LOAN_RENEWED:  return "续借";
        default:            return "借出";
    }
}
static LoanStatus loan_status_code(const char *s) {
    if (strcmp(s, "已还") == 0) return LOAN_RETURNED;
    if (strcmp(s, "逾期") == 0) return LOAN_OVERDUE;
    if (strcmp(s, "续借") == 0) return LOAN_RENEWED;
    return LOAN_ACTIVE;
}

/* 预约状态：0排队 / 1可借 / 2已取 / 3过期 / 4取消 */
static const char *reservation_status_text(ReservationStatus s) {
    switch (s) {
        case RES_AVAILABLE: return "可借";
        case RES_PICKED:    return "已取";
        case RES_EXPIRED:   return "过期";
        case RES_CANCELLED: return "取消";
        default:            return "排队";
    }
}
static ReservationStatus reservation_status_code(const char *s) {
    if (strcmp(s, "可借") == 0) return RES_AVAILABLE;
    if (strcmp(s, "已取") == 0) return RES_PICKED;
    if (strcmp(s, "过期") == 0) return RES_EXPIRED;
    if (strcmp(s, "取消") == 0) return RES_CANCELLED;
    return RES_WAITING;
}

/* 罚款状态：0未缴 / 1已缴 */
static const char *fine_status_text(FineStatus s) {
    switch (s) {
        case FINE_PAID: return "已缴";
        default:        return "未缴";
    }
}
static FineStatus fine_status_code(const char *s) {
    if (strcmp(s, "已缴") == 0) return FINE_PAID;
    return FINE_UNPAID;
}

/* ============ meta 键值操作 ============ */

static const char *meta_get(const char *key) {
    size_t i;
    if (!key) return NULL;
    for (i = 0; i < s_meta_count; i++)
        if (strcmp(s_meta[i].key, key) == 0) return s_meta[i].value;
    return NULL;
}

static int meta_get_int(const char *key, int def) {
    const char *v = meta_get(key);
    return v ? atoi(v) : def;
}

static Result meta_set(const char *key, const char *value) {
    size_t i;
    if (!key || !value) return RES_ERR_INVALID;
    for (i = 0; i < s_meta_count; i++) {
        if (strcmp(s_meta[i].key, key) == 0) {
            str_copy(s_meta[i].value, sizeof s_meta[i].value, value);
            return RES_OK;
        }
    }
    if (s_meta_count >= META_CAP) return RES_ERR_LIMIT;
    str_copy(s_meta[i].key, sizeof s_meta[i].key, key);
    str_copy(s_meta[i].value, sizeof s_meta[i].value, value);
    s_meta_count++;
    return RES_OK;
}

static Result meta_set_int(const char *key, int v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%d", v);
    return meta_set(key, buf);
}

/* ============ CSV 表构造 / 写出 辅助 ============ */

/* 将一行字段（表头或数据行）追加到 CsvTable（字段会被复制） */
static Result row_to_table(CsvTable *t, const char *const *fields, size_t n) {
    CsvRow row = {0};
    size_t i;
    Result r = RES_OK;

    for (i = 0; i < n; i++) {
        const char *s = fields[i] ? fields[i] : "";
        size_t sl = strlen(s);
        char *dup = malloc(sl + 1);
        if (!dup) { r = RES_ERR_NO_MEM; goto fail; }
        memcpy(dup, s, sl + 1);
        if (row.count == row.cap) {
            size_t nc = row.cap ? row.cap * 2 : 8;
            char **nf = realloc(row.fields, nc * sizeof(char *));
            if (!nf) { free(dup); r = RES_ERR_NO_MEM; goto fail; }
            row.fields = nf;
            row.cap = nc;
        }
        row.fields[row.count++] = dup;
    }
    if (t->count == t->cap) {
        size_t nc = t->cap ? t->cap * 2 : 16;
        CsvRow *nr = realloc(t->rows, nc * sizeof(CsvRow));
        if (!nr) { r = RES_ERR_NO_MEM; goto fail; }
        t->rows = nr;
        t->cap = nc;
    }
    t->rows[t->count++] = row;
    return RES_OK;

fail:
    while (row.count > 0) free(row.fields[--row.count]);
    free(row.fields);
    return r;
}

/* 整表序列化后加 BOM 原子写盘（BOM + 表头 + 数据行，每行 \n 结尾） */
static Result table_to_file(const char *fname, const CsvTable *t) {
    char path[512];
    char *text = NULL, *full = NULL;
    size_t textlen;
    Result r;

    if ((r = csv_table_write(t, &text)) != RES_OK) return r;
    textlen = strlen(text);
    full = malloc(textlen + 4);
    if (!full) { free(text); return RES_ERR_NO_MEM; }
    full[0] = BOM_BYTE0;
    full[1] = BOM_BYTE1;
    full[2] = BOM_BYTE2;
    memcpy(full + 3, text, textlen);

    snprintf(path, sizeof path, "%s/%s", REPO_DATA_DIR, fname);
    r = file_atomic_write(path, full, textlen + 3);
    free(full);
    free(text);
    return r;
}

/* 行写出回调：把第 idx 条记录的各字段指针填入 fields（须指向稳定内存） */
typedef Result (*RowWriter)(const DynArray *arr, size_t idx, const char **fields);

/* 通用写表：构造表头 + 各行，BOM + 原子写盘 */
static Result write_table_file(const char *fname, const char *const *header,
                               size_t ncols, const DynArray *arr,
                               RowWriter writer) {
    CsvTable t = {0};
    Result r;
    size_t i;

    if ((r = row_to_table(&t, header, ncols)) != RES_OK) goto done;
    for (i = 0; i < arr->len; i++) {
        const char *fields[16];
        if ((r = writer(arr, i, fields)) != RES_OK) goto done;
        if ((r = row_to_table(&t, fields, ncols)) != RES_OK) goto done;
    }
    r = table_to_file(fname, &t);
done:
    csv_table_free(&t);
    return r;
}

/* 行解析回调：把一行字段映射为实体并追加到集合 */
typedef Result (*RowParser)(const CsvRow *row);

/* 通用读表：读全文 → 跳过 BOM → csv_parse → 跳过表头行映射数据行。
   文件不存在 = 空表（不报错、不创建）。 */
static Result read_table_file(const char *fname, size_t min_cols,
                              RowParser parser) {
    char path[512];
    char *text;
    size_t len = 0;
    CsvTable t = {0};
    Result r = RES_OK;
    size_t i;

    snprintf(path, sizeof path, "%s/%s", REPO_DATA_DIR, fname);
    text = file_read_all(path, &len);
    if (!text) return RES_OK; /* 文件不存在 */
    if (len >= 3 && (unsigned char)text[0] == (unsigned char)BOM_BYTE0 &&
        (unsigned char)text[1] == (unsigned char)BOM_BYTE1 &&
        (unsigned char)text[2] == (unsigned char)BOM_BYTE2)
        memmove(text, text + 3, len - 2); /* 跳过 BOM */
    r = csv_parse(text, &t);
    free(text);
    if (r != RES_OK) return r;

    for (i = 1; i < t.count; i++) { /* rows[0] 为表头，跳过 */
        if (t.rows[i].count < min_cols) continue; /* 列数不足，容错跳过 */
        if ((r = parser(&t.rows[i])) != RES_OK) break;
    }
    csv_table_free(&t);
    return r;
}

/* ============ 各表：行写出 / 行解析 ============ */

/* books.csv */
static Result book_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Book *b = (const Book *)arr->data + idx;
    static char buf_id[16], buf_author[16], buf_publisher[16], buf_category[16],
                buf_price[32], buf_total[16], buf_avail[16];
    snprintf(buf_id, sizeof buf_id, "%d", b->id);
    snprintf(buf_author, sizeof buf_author, "%d", b->author_id);
    snprintf(buf_publisher, sizeof buf_publisher, "%d", b->publisher_id);
    snprintf(buf_category, sizeof buf_category, "%d", b->category_id);
    snprintf(buf_price, sizeof buf_price, "%g", b->price);
    snprintf(buf_total, sizeof buf_total, "%d", b->total_copies);
    snprintf(buf_avail, sizeof buf_avail, "%d", b->available_copies);
    fields[0] = buf_id;
    fields[1] = b->isbn;
    fields[2] = b->title;
    fields[3] = buf_author;
    fields[4] = buf_publisher;
    fields[5] = buf_category;
    fields[6] = b->pub_date;
    fields[7] = buf_price;
    fields[8] = buf_total;
    fields[9] = buf_avail;
    fields[10] = b->location;
    fields[11] = book_status_text(b->status);
    fields[12] = b->description;
    fields[13] = b->created_at;
    return RES_OK;
}

static Result book_parse(const CsvRow *row) {
    Book b;
    memset(&b, 0, sizeof b);
    b.id = atoi(row->fields[0]);
    str_copy(b.isbn, sizeof b.isbn, row->fields[1]);
    str_copy(b.title, sizeof b.title, row->fields[2]);
    b.author_id = atoi(row->fields[3]);
    b.publisher_id = atoi(row->fields[4]);
    b.category_id = atoi(row->fields[5]);
    str_copy(b.pub_date, sizeof b.pub_date, row->fields[6]);
    b.price = strtod(row->fields[7], NULL);
    b.total_copies = atoi(row->fields[8]);
    b.available_copies = atoi(row->fields[9]);
    str_copy(b.location, sizeof b.location, row->fields[10]);
    b.status = book_status_code(row->fields[11]);
    str_copy(b.description, sizeof b.description, row->fields[12]);
    str_copy(b.created_at, sizeof b.created_at, row->fields[13]);
    return dynarray_push(&s_books, &b);
}

/* readers.csv */
static Result reader_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Reader *r = (const Reader *)arr->data + idx;
    static char buf_id[16];
    snprintf(buf_id, sizeof buf_id, "%d", r->id);
    fields[0] = buf_id;
    fields[1] = r->reader_no;
    fields[2] = r->name;
    fields[3] = r->password_hash;
    fields[4] = r->salt;
    fields[5] = r->phone;
    fields[6] = r->email;
    fields[7] = reader_status_text(r->status);
    fields[8] = r->created_at;
    return RES_OK;
}

static Result reader_parse(const CsvRow *row) {
    Reader r;
    memset(&r, 0, sizeof r);
    r.id = atoi(row->fields[0]);
    str_copy(r.reader_no, sizeof r.reader_no, row->fields[1]);
    str_copy(r.name, sizeof r.name, row->fields[2]);
    str_copy(r.password_hash, sizeof r.password_hash, row->fields[3]);
    str_copy(r.salt, sizeof r.salt, row->fields[4]);
    str_copy(r.phone, sizeof r.phone, row->fields[5]);
    str_copy(r.email, sizeof r.email, row->fields[6]);
    r.status = reader_status_code(row->fields[7]);
    str_copy(r.created_at, sizeof r.created_at, row->fields[8]);
    return dynarray_push(&s_readers, &r);
}

/* authors.csv */
static Result author_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Author *a = (const Author *)arr->data + idx;
    static char buf_id[16], buf_birth[16];
    snprintf(buf_id, sizeof buf_id, "%d", a->id);
    snprintf(buf_birth, sizeof buf_birth, "%d", a->birth_year);
    fields[0] = buf_id;
    fields[1] = a->name;
    fields[2] = buf_birth;
    fields[3] = a->nationality;
    fields[4] = a->description;
    return RES_OK;
}

static Result author_parse(const CsvRow *row) {
    Author a;
    memset(&a, 0, sizeof a);
    a.id = atoi(row->fields[0]);
    str_copy(a.name, sizeof a.name, row->fields[1]);
    a.birth_year = atoi(row->fields[2]);
    str_copy(a.nationality, sizeof a.nationality, row->fields[3]);
    str_copy(a.description, sizeof a.description, row->fields[4]);
    return dynarray_push(&s_authors, &a);
}

/* publishers.csv */
static Result publisher_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Publisher *p = (const Publisher *)arr->data + idx;
    static char buf_id[16];
    snprintf(buf_id, sizeof buf_id, "%d", p->id);
    fields[0] = buf_id;
    fields[1] = p->name;
    fields[2] = p->address;
    fields[3] = p->phone;
    return RES_OK;
}

static Result publisher_parse(const CsvRow *row) {
    Publisher p;
    memset(&p, 0, sizeof p);
    p.id = atoi(row->fields[0]);
    str_copy(p.name, sizeof p.name, row->fields[1]);
    str_copy(p.address, sizeof p.address, row->fields[2]);
    str_copy(p.phone, sizeof p.phone, row->fields[3]);
    return dynarray_push(&s_publishers, &p);
}

/* categories.csv */
static Result category_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Category *c = (const Category *)arr->data + idx;
    static char buf_id[16], buf_parent[16];
    snprintf(buf_id, sizeof buf_id, "%d", c->id);
    snprintf(buf_parent, sizeof buf_parent, "%d", c->parent_id);
    fields[0] = buf_id;
    fields[1] = c->name;
    fields[2] = buf_parent;
    return RES_OK;
}

static Result category_parse(const CsvRow *row) {
    Category c;
    memset(&c, 0, sizeof c);
    c.id = atoi(row->fields[0]);
    str_copy(c.name, sizeof c.name, row->fields[1]);
    c.parent_id = atoi(row->fields[2]);
    return dynarray_push(&s_categories, &c);
}

/* loans.csv */
static Result loan_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Loan *l = (const Loan *)arr->data + idx;
    static char buf_id[16], buf_book[16], buf_copy[16], buf_reader[16],
                buf_renew[16];
    snprintf(buf_id, sizeof buf_id, "%d", l->id);
    snprintf(buf_book, sizeof buf_book, "%d", l->book_id);
    snprintf(buf_copy, sizeof buf_copy, "%d", l->copy_no);
    snprintf(buf_reader, sizeof buf_reader, "%d", l->reader_id);
    snprintf(buf_renew, sizeof buf_renew, "%d", l->renew_count);
    fields[0] = buf_id;
    fields[1] = buf_book;
    fields[2] = buf_copy;
    fields[3] = buf_reader;
    fields[4] = l->borrow_date;
    fields[5] = l->due_date;
    fields[6] = l->return_date;
    fields[7] = loan_status_text(l->status);
    fields[8] = buf_renew;
    return RES_OK;
}

static Result loan_parse(const CsvRow *row) {
    Loan l;
    memset(&l, 0, sizeof l);
    l.id = atoi(row->fields[0]);
    l.book_id = atoi(row->fields[1]);
    l.copy_no = atoi(row->fields[2]);
    l.reader_id = atoi(row->fields[3]);
    str_copy(l.borrow_date, sizeof l.borrow_date, row->fields[4]);
    str_copy(l.due_date, sizeof l.due_date, row->fields[5]);
    str_copy(l.return_date, sizeof l.return_date, row->fields[6]);
    l.status = loan_status_code(row->fields[7]);
    l.renew_count = atoi(row->fields[8]);
    return dynarray_push(&s_loans, &l);
}

/* reservations.csv */
static Result reservation_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Reservation *r = (const Reservation *)arr->data + idx;
    static char buf_id[16], buf_book[16], buf_reader[16];
    snprintf(buf_id, sizeof buf_id, "%d", r->id);
    snprintf(buf_book, sizeof buf_book, "%d", r->book_id);
    snprintf(buf_reader, sizeof buf_reader, "%d", r->reader_id);
    fields[0] = buf_id;
    fields[1] = buf_book;
    fields[2] = buf_reader;
    fields[3] = r->reserve_date;
    fields[4] = r->expire_date;
    fields[5] = reservation_status_text(r->status);
    return RES_OK;
}

static Result reservation_parse(const CsvRow *row) {
    Reservation r;
    memset(&r, 0, sizeof r);
    r.id = atoi(row->fields[0]);
    r.book_id = atoi(row->fields[1]);
    r.reader_id = atoi(row->fields[2]);
    str_copy(r.reserve_date, sizeof r.reserve_date, row->fields[3]);
    str_copy(r.expire_date, sizeof r.expire_date, row->fields[4]);
    r.status = reservation_status_code(row->fields[5]);
    return dynarray_push(&s_reservations, &r);
}

/* fines.csv */
static Result fine_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Fine *f = (const Fine *)arr->data + idx;
    static char buf_id[16], buf_reader[16], buf_loan[16], buf_amount[32];
    snprintf(buf_id, sizeof buf_id, "%d", f->id);
    snprintf(buf_reader, sizeof buf_reader, "%d", f->reader_id);
    snprintf(buf_loan, sizeof buf_loan, "%d", f->loan_id);
    snprintf(buf_amount, sizeof buf_amount, "%g", f->amount);
    fields[0] = buf_id;
    fields[1] = buf_reader;
    fields[2] = buf_loan;
    fields[3] = buf_amount;
    fields[4] = f->created_at;
    fields[5] = fine_status_text(f->status);
    fields[6] = f->paid_at;
    return RES_OK;
}

static Result fine_parse(const CsvRow *row) {
    Fine f;
    memset(&f, 0, sizeof f);
    f.id = atoi(row->fields[0]);
    f.reader_id = atoi(row->fields[1]);
    f.loan_id = atoi(row->fields[2]);
    f.amount = strtod(row->fields[3], NULL);
    str_copy(f.created_at, sizeof f.created_at, row->fields[4]);
    f.status = fine_status_code(row->fields[5]);
    str_copy(f.paid_at, sizeof f.paid_at, row->fields[6]);
    return dynarray_push(&s_fines, &f);
}

/* admins.csv（扩展表） */
static Result admin_writer(const DynArray *arr, size_t idx, const char **fields) {
    const Admin *a = (const Admin *)arr->data + idx;
    static char buf_id[16];
    snprintf(buf_id, sizeof buf_id, "%d", a->id);
    fields[0] = buf_id;
    fields[1] = a->username;
    fields[2] = a->password_hash;
    fields[3] = a->salt;
    fields[4] = a->created_at;
    return RES_OK;
}

static Result admin_parse(const CsvRow *row) {
    Admin a;
    memset(&a, 0, sizeof a);
    a.id = atoi(row->fields[0]);
    str_copy(a.username, sizeof a.username, row->fields[1]);
    str_copy(a.password_hash, sizeof a.password_hash, row->fields[2]);
    str_copy(a.salt, sizeof a.salt, row->fields[3]);
    str_copy(a.created_at, sizeof a.created_at, row->fields[4]);
    return dynarray_push(&s_admins, &a);
}

/* meta.csv */
static Result meta_load(void) {
    char path[512];
    char *text;
    size_t len = 0;
    CsvTable t = {0};
    Result r;
    size_t i;

    snprintf(path, sizeof path, "%s/%s", REPO_DATA_DIR, "meta.csv");
    text = file_read_all(path, &len);
    if (text) {
        if (len >= 3 && (unsigned char)text[0] == (unsigned char)BOM_BYTE0 &&
            (unsigned char)text[1] == (unsigned char)BOM_BYTE1 &&
            (unsigned char)text[2] == (unsigned char)BOM_BYTE2)
            memmove(text, text + 3, len - 2);
        r = csv_parse(text, &t);
        free(text);
        if (r != RES_OK) return r;
        for (i = 1; i < t.count; i++) {
            if (t.rows[i].count < 2) continue;
            meta_set(t.rows[i].fields[0], t.rows[i].fields[1]);
        }
        csv_table_free(&t);
    }
    /* 文件缺失或键缺失时补齐默认键值 */
    for (i = 0; i < META_DEFAULT_COUNT; i++)
        if (!meta_get(META_DEFAULTS[i].key))
            meta_set(META_DEFAULTS[i].key, META_DEFAULTS[i].value);
    return RES_OK;
}

static Result save_meta(void) {
    static const char *const hdr[2] = { "key", "value" };
    CsvTable t = {0};
    Result r;
    size_t i;

    if ((r = row_to_table(&t, hdr, 2)) != RES_OK) goto done;
    for (i = 0; i < s_meta_count; i++) {
        const char *f[2] = { s_meta[i].key, s_meta[i].value };
        if ((r = row_to_table(&t, f, 2)) != RES_OK) goto done;
    }
    r = table_to_file("meta.csv", &t);
done:
    csv_table_free(&t);
    return r;
}

/* ============ 计数校正：next_* 至少为 max(id)+1 ============ */
static void correct_counter(const char *key, const DynArray *arr) {
    size_t i;
    int mx = 0, cur;
    /* 各实体结构均以 EntityId id 开头：必须按 elem_size 步长取首字段 */
    for (i = 0; i < arr->len; i++) {
        const EntityId *pid =
            (const EntityId *)((const char *)arr->data + i * arr->elem_size);
        if (*pid > mx) mx = *pid;
    }
    cur = meta_get_int(key, 1);
    if (cur < mx + 1) meta_set_int(key, mx + 1);
}

/* ============ 加载 / 保存 ============ */

Result repo_load_all(void) {
    Result r;

    /* 若已加载过则先释放旧集合（支持重复调用） */
    dynarray_free(&s_books);
    dynarray_free(&s_readers);
    dynarray_free(&s_authors);
    dynarray_free(&s_publishers);
    dynarray_free(&s_categories);
    dynarray_free(&s_loans);
    dynarray_free(&s_reservations);
    dynarray_free(&s_fines);
    dynarray_free(&s_admins);
    s_meta_count = 0;

    /* 确保数据目录存在 */
    if ((r = file_ensure_dir(REPO_DATA_DIR)) != RES_OK) return r;

    /* 初始化 9 个集合 */
    dynarray_init(&s_books, sizeof(Book));
    dynarray_init(&s_readers, sizeof(Reader));
    dynarray_init(&s_authors, sizeof(Author));
    dynarray_init(&s_publishers, sizeof(Publisher));
    dynarray_init(&s_categories, sizeof(Category));
    dynarray_init(&s_loans, sizeof(Loan));
    dynarray_init(&s_reservations, sizeof(Reservation));
    dynarray_init(&s_fines, sizeof(Fine));
    dynarray_init(&s_admins, sizeof(Admin));

    /* 逐个加载 CSV（文件不存在 = 空表） */
    if ((r = read_table_file("books.csv", BOOKS_COLS, book_parse)) != RES_OK) return r;
    if ((r = read_table_file("readers.csv", READERS_COLS, reader_parse)) != RES_OK) return r;
    if ((r = read_table_file("authors.csv", AUTHORS_COLS, author_parse)) != RES_OK) return r;
    if ((r = read_table_file("publishers.csv", PUBLISHERS_COLS, publisher_parse)) != RES_OK) return r;
    if ((r = read_table_file("categories.csv", CATEGORIES_COLS, category_parse)) != RES_OK) return r;
    if ((r = read_table_file("loans.csv", LOANS_COLS, loan_parse)) != RES_OK) return r;
    if ((r = read_table_file("reservations.csv", RESERVATIONS_COLS, reservation_parse)) != RES_OK) return r;
    if ((r = read_table_file("fines.csv", FINES_COLS, fine_parse)) != RES_OK) return r;
    if ((r = read_table_file("admins.csv", ADMINS_COLS, admin_parse)) != RES_OK) return r;

    /* 加载 meta（缺失时补齐默认键值） */
    if ((r = meta_load()) != RES_OK) return r;

    /* 校正计数：每张表的 next_* 至少为 max(id)+1 */
    correct_counter("next_book_id", &s_books);
    correct_counter("next_reader_id", &s_readers);
    correct_counter("next_author_id", &s_authors);
    correct_counter("next_publisher_id", &s_publishers);
    correct_counter("next_category_id", &s_categories);
    correct_counter("next_loan_id", &s_loans);
    correct_counter("next_reservation_id", &s_reservations);
    correct_counter("next_fine_id", &s_fines);
    correct_counter("next_admin_id", &s_admins);

    return RES_OK;
}

Result repo_save_all(void) {
    Result r;

    if ((r = file_ensure_dir(REPO_DATA_DIR)) != RES_OK) return r;

    if ((r = write_table_file("books.csv", BOOKS_HEADER, BOOKS_COLS,
                              &s_books, book_writer)) != RES_OK) return r;
    if ((r = write_table_file("readers.csv", READERS_HEADER, READERS_COLS,
                              &s_readers, reader_writer)) != RES_OK) return r;
    if ((r = write_table_file("authors.csv", AUTHORS_HEADER, AUTHORS_COLS,
                              &s_authors, author_writer)) != RES_OK) return r;
    if ((r = write_table_file("publishers.csv", PUBLISHERS_HEADER, PUBLISHERS_COLS,
                              &s_publishers, publisher_writer)) != RES_OK) return r;
    if ((r = write_table_file("categories.csv", CATEGORIES_HEADER, CATEGORIES_COLS,
                              &s_categories, category_writer)) != RES_OK) return r;
    if ((r = write_table_file("loans.csv", LOANS_HEADER, LOANS_COLS,
                              &s_loans, loan_writer)) != RES_OK) return r;
    if ((r = write_table_file("reservations.csv", RESERVATIONS_HEADER, RESERVATIONS_COLS,
                              &s_reservations, reservation_writer)) != RES_OK) return r;
    if ((r = write_table_file("fines.csv", FINES_HEADER, FINES_COLS,
                              &s_fines, fine_writer)) != RES_OK) return r;
    if ((r = write_table_file("admins.csv", ADMINS_HEADER, ADMINS_COLS,
                              &s_admins, admin_writer)) != RES_OK) return r;
    return save_meta();
}

/* ---- 集合访问 ---- */
#define ACCESSOR(name, svar, fname)                          \
    const name *repo_##fname(size_t *count) {                \
        if (count) *count = svar.len;                        \
        return (const name *)svar.data;                      \
    }

ACCESSOR(Book, s_books, books)
ACCESSOR(Reader, s_readers, readers)
ACCESSOR(Author, s_authors, authors)
ACCESSOR(Publisher, s_publishers, publishers)
ACCESSOR(Category, s_categories, categories)
ACCESSOR(Loan, s_loans, loans)
ACCESSOR(Reservation, s_reservations, reservations)
ACCESSOR(Fine, s_fines, fines)
ACCESSOR(Admin, s_admins, admins)

/* ---- 新增（自动分配 ID、追加、写盘；*out 指向仓库内记录） ---- */
#define ADDER(name, svar, fname, ckey)                                          \
    Result repo_add_##fname(const name *src, name **out) {                      \
        name copy;                                                              \
        Result r;                                                               \
        if (!src) return RES_ERR_INVALID;                                       \
        if (out) *out = NULL;                                                   \
        copy = *src;                                                            \
        if (copy.id <= 0) {                                                     \
            copy.id = repo_config_int(ckey, 1);                                 \
            meta_set_int(ckey, copy.id + 1);                                    \
        }                                                                       \
        if (dynarray_push(&svar, &copy) != RES_OK) return RES_ERR_NO_MEM;       \
        if (out) *out = (name *)dynarray_get(&svar, svar.len - 1);              \
        r = repo_save_all();                                                    \
        return r;                                                               \
    }

ADDER(Book, s_books, book, "next_book_id")
ADDER(Reader, s_readers, reader, "next_reader_id")
ADDER(Author, s_authors, author, "next_author_id")
ADDER(Publisher, s_publishers, publisher, "next_publisher_id")
ADDER(Category, s_categories, category, "next_category_id")
ADDER(Loan, s_loans, loan, "next_loan_id")
ADDER(Reservation, s_reservations, reservation, "next_reservation_id")
ADDER(Fine, s_fines, fine, "next_fine_id")
ADDER(Admin, s_admins, admin, "next_admin_id")

/* ---- 按 ID 查找（返回内部指针或 NULL） ---- */
#define FINDER(name, svar, fname)                            \
    name *repo_find_##fname(EntityId id) {                   \
        const name *arr = (const name *)svar.data;           \
        size_t i;                                            \
        for (i = 0; i < svar.len; i++)                       \
            if (arr[i].id == id) return (name *)&arr[i];     \
        return NULL;                                         \
    }

FINDER(Book, s_books, book)
FINDER(Reader, s_readers, reader)
FINDER(Author, s_authors, author)
FINDER(Publisher, s_publishers, publisher)
FINDER(Category, s_categories, category)
FINDER(Loan, s_loans, loan)
FINDER(Reservation, s_reservations, reservation)
FINDER(Fine, s_fines, fine)
FINDER(Admin, s_admins, admin)

/* ---- 硬删除（字典表：按 ID 移除元素并保存） ---- */
#define REMOVER(name, svar, fname)                                      \
    Result repo_remove_##fname(EntityId id) {                           \
        name *arr = (name *)svar.data;                                  \
        size_t i;                                                       \
        for (i = 0; i < svar.len; i++)                                  \
            if (arr[i].id == id) {                                      \
                memmove(&arr[i], &arr[i + 1],                           \
                        (svar.len - i - 1) * sizeof(name));             \
                svar.len--;                                             \
                return repo_save_all();                                 \
            }                                                           \
        return RES_ERR_NOT_FOUND;                                       \
    }

REMOVER(Author, s_authors, author)
REMOVER(Publisher, s_publishers, publisher)
REMOVER(Category, s_categories, category)

/* ---- 按业务键查找 ---- */
Reader *repo_find_reader_by_no(const char *reader_no) {
    const Reader *arr = (const Reader *)s_readers.data;
    size_t i;
    if (!reader_no) return NULL;
    for (i = 0; i < s_readers.len; i++)
        if (strcmp(arr[i].reader_no, reader_no) == 0) return (Reader *)&arr[i];
    return NULL;
}

Admin *repo_find_admin_by_username(const char *username) {
    const Admin *arr = (const Admin *)s_admins.data;
    size_t i;
    if (!username) return NULL;
    for (i = 0; i < s_admins.len; i++)
        if (strcmp(arr[i].username, username) == 0) return (Admin *)&arr[i];
    return NULL;
}

/* ---- 配置（meta.csv） ---- */
int repo_config_int(const char *key, int def) {
    const char *v = meta_get(key);
    return v ? atoi(v) : def;
}

double repo_config_double(const char *key, double def) {
    const char *v = meta_get(key);
    return v ? strtod(v, NULL) : def;
}

#include "migration/library.h"
#include <string.h>

int cl_library_number(const ClRow *r,int field) {
    int n=0;if (r && field>=0 && field<r->count) cl_int_parse(r->field[field],&n);return n;
}
int cl_library_find(const ClStore *s,int k,int id) {
    if (!s || k<0 || k>=CL_TABLES || id<=0) return -1;
    for (int i=0;i<s->table[k].count;++i) if (cl_library_number(&s->table[k].rows[i],0)==id) return i;
    return -1;
}
static int next_id(const ClTable *t) {
    int max=0;for (int i=0;i<t->count;++i) {
        int id=cl_library_number(&t->rows[i],0);if (id>max) max=id;
    }
    return max==CL_INT_MAX?0:max+1;
}
static int same(const ClRow *r,int col,const char *v) { return !strcmp(r->field[col],v); }
static void set(ClRow *r,int col,const char *v) { text_copy(r->field[col],512,v); }
static void num(ClRow *r,int col,int n) { cl_int_format(r->field[col],512,n); }
static int admin(const ClSession *a) {
    return a && a->authenticated && a->role==CL_ROLE_ADMIN && a->reader_id==0 && !strcmp(a->username,"admin");
}
static int allowed(const ClStore *s,const ClSession *a,int reader,int normal) {
    if (admin(a)) return 1;
    if (!a || !a->authenticated || a->role!=CL_ROLE_READER || a->reader_id!=reader) return 0;
    int i=cl_library_find(s,CL_READERS,reader);if (i<0) return 0;
    const ClRow *r=&s->table[0].rows[i];
    return !strcmp(a->username,r->field[1]) && !same(r,7,"注销") && (!normal || same(r,7,"正常"));
}
static int normal_reader(const ClStore *s,int id) {
    int i=cl_library_find(s,0,id);return i>=0 && same(&s->table[0].rows[i],7,"正常");
}
static int reservation_open(const ClRow *r) { return same(r,5,"waiting") || same(r,5,"ready"); }
static int allocate(ClStore *s,int book,ClDate today,int *changed) {
    int bi=cl_library_find(s,CL_BOOKS,book);if (bi<0) return 0;
    ClRow *b=&s->table[1].rows[bi];ClDate expires;char text[11];
    while (cl_library_number(b,5)>0) {
        int selected=-1,best=CL_INT_MAX;
        for (int i=0;i<s->table[4].count;++i) {
            ClRow *r=&s->table[4].rows[i];int id=cl_library_number(r,0);
            if (cl_library_number(r,1)==book && same(r,5,"waiting") &&
                (selected<0 || id<best)) { selected=i;best=id; }
        }
        if (selected<0) break;
        if (!cl_date_add(today,2,&expires) || !cl_date_format(text,11,expires)) return 0;
        ClRow *r=&s->table[4].rows[selected];set(r,5,"ready");set(r,4,text);
        num(b,5,cl_library_number(b,5)-1);*changed=1;
    }
    return 1;
}
static int sweep(ClStore *s,ClDate today,int *changed) {
    if (!cl_date_valid(today)) return 0;
    for (int i=0;i<s->table[4].count;++i) {
        ClRow *r=&s->table[4].rows[i];ClDate expiry;int days;
        if (!same(r,5,"ready")) continue;
        if (!cl_date_parse(r->field[4],&expiry) || !cl_date_difference(today,expiry,&days)) return 0;
        if (days>0) {
            int bi=cl_library_find(s,1,cl_library_number(r,1));if (bi<0) return 0;
            ClRow *b=&s->table[1].rows[bi];int available=cl_library_number(b,5);
            if (available>=cl_library_number(b,4)) return 0;
            set(r,5,"expired");num(b,5,available+1);*changed=1;
        }
    }
    for (int i=0;i<s->table[1].count;++i)
        if (!allocate(s,cl_library_number(&s->table[1].rows[i],0),today,changed)) return 0;
    return 1;
}
static int begin(ClStore *s,ClStore *work,ClDate today) {
    if (!s || s->blocked || !cl_date_valid(today) || !cl_store_open(work,s->directory)) return 0;
    int changed=0;
    if (!sweep(work,today,&changed)) { cl_store_dispose(work);return 0; }
    return 1;
}
static int finish(ClStore *s,ClStore *w,int result) {
    if (result!=CL_LIB_OK) { cl_store_dispose(w);return result; }
    int status=cl_store_commit(w);
    if (status==CL_STORE_FAILED) { cl_store_dispose(w);return CL_LIB_STORAGE; }
    w->recovered=w->recovered || s->recovered;
    cl_store_dispose(s);*s=*w;
    if (status==CL_STORE_UNCERTAIN) return CL_LIB_UNCERTAIN;
    return status==CL_STORE_PENDING_EXPORT?CL_LIB_PENDING:CL_LIB_OK;
}
int cl_library_refresh(ClStore *s,ClDate today) {
    if (!s || !cl_date_valid(today)) return CL_LIB_INVALID;
    ClStore loaded={0};int changed=0;
    if (!cl_store_open(&loaded,s->directory)) { s->blocked=1;return CL_LIB_STORAGE; }
    if (!sweep(&loaded,today,&changed)) { cl_store_dispose(&loaded);return CL_LIB_INVALID; }
    if (changed) return finish(s,&loaded,CL_LIB_OK);
    loaded.recovered=loaded.recovered || s->recovered;
    cl_store_dispose(s);*s=loaded;return CL_LIB_OK;
}
int cl_library_overdue(const ClRow *loan,ClDate today) {
    ClDate due;int days=0;
    if (!loan || !same(loan,6,"active") || !cl_date_parse(loan->field[4],&due) ||
        !cl_date_difference(today,due,&days)) return 0;
    return days>0?days:0;
}
static int borrow_inner(ClStore *w,const ClSession *a,int reader,int book,ClDate today,int *out) {
    if (!allowed(w,a,reader,1)) return CL_LIB_DENIED;
    if (!normal_reader(w,reader)) return CL_LIB_BLOCKED;
    int bi=cl_library_find(w,1,book);if (bi<0) return CL_LIB_NOT_FOUND;
    ClRow *b=&w->table[1].rows[bi];if (!same(b,6,"available")) return CL_LIB_NO_COPY;
    int active=0;
    for (int i=0;i<w->table[2].count;++i) {
        ClRow *l=&w->table[2].rows[i];
        if (cl_library_number(l,2)==reader && same(l,6,"active")) {
            if (cl_library_number(l,1)==book) return CL_LIB_DUPLICATE;
            ++active;
        }
    }
    if (active>=5) return CL_LIB_LIMIT;
    long long unpaid=0;
    for (int i=0;i<w->table[3].count;++i) {
        ClRow *f=&w->table[3].rows[i];
        if (cl_library_number(f,1)==reader && same(f,5,"unpaid")) unpaid+=cl_library_number(f,3);
    }
    if (unpaid>=1000) return CL_LIB_BLOCKED;
    int ready=-1;
    for (int i=0;i<w->table[4].count;++i) {
        ClRow *r=&w->table[4].rows[i];
        if (cl_library_number(r,1)==book && cl_library_number(r,2)==reader && same(r,5,"ready")) ready=i;
    }
    if (ready<0 && cl_library_number(b,5)<=0) return CL_LIB_NO_COPY;
    ClDate due;ClRow l={0};l.count=8;int id=next_id(&w->table[2]);
    if (!id || !cl_date_add(today,30,&due) || !cl_date_format(l.field[3],512,today) ||
        !cl_date_format(l.field[4],512,due)) return CL_LIB_INVALID;
    num(&l,0,id);num(&l,1,book);num(&l,2,reader);set(&l,6,"active");num(&l,7,0);
    if (!cl_table_append(&w->table[2],&l)) return CL_LIB_STORAGE;
    if (ready>=0) set(&w->table[4].rows[ready],5,"fulfilled");
    else num(b,5,cl_library_number(b,5)-1);
    if (out) *out=id;
    return CL_LIB_OK;
}
int cl_library_borrow(ClStore *s,const ClSession *a,int reader,int book,ClDate today,int *out) {
    ClStore w={0};int id=0;if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int result=finish(s,&w,borrow_inner(&w,a,reader,book,today,&id));
    if ((result==CL_LIB_OK || result==CL_LIB_PENDING) && out) *out=id;
    return result;
}
static int return_inner(ClStore *w,const ClSession *a,int id,ClDate today,int *amount) {
    int li=cl_library_find(w,2,id);if (li<0) return CL_LIB_NOT_FOUND;
    ClRow *l=&w->table[2].rows[li];
    if (!allowed(w,a,cl_library_number(l,2),0)) return CL_LIB_DENIED;
    if (!same(l,6,"active")) return CL_LIB_CLOSED;
    int bi=cl_library_find(w,1,cl_library_number(l,1));if (bi<0) return CL_LIB_NOT_FOUND;
    ClRow *b=&w->table[1].rows[bi];int available=cl_library_number(b,5);
    if (available>=cl_library_number(b,4)) return CL_LIB_INVALID;
    int cents=cl_library_overdue(l,today)*50;
    if (cents) {
        ClRow f={0};f.count=7;int fid=next_id(&w->table[3]);if (!fid) return CL_LIB_INVALID;
        num(&f,0,fid);num(&f,1,cl_library_number(l,2));num(&f,2,id);num(&f,3,cents);
        set(&f,5,"unpaid");cl_date_format(f.field[4],512,today);
        if (!cl_table_append(&w->table[3],&f)) return CL_LIB_STORAGE;
    }
    cl_date_format(l->field[5],512,today);set(l,6,"returned");num(b,5,available+1);
    int changed=0;if (!allocate(w,cl_library_number(b,0),today,&changed)) return CL_LIB_INVALID;
    if (amount) *amount=cents;
    return CL_LIB_OK;
}
int cl_library_return(ClStore *s,const ClSession *a,int id,ClDate today,int *amount) {
    ClStore w={0};int cents=0;if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int result=finish(s,&w,return_inner(&w,a,id,today,&cents));
    if ((result==CL_LIB_OK || result==CL_LIB_PENDING) && amount) *amount=cents;
    return result;
}
int cl_library_renew(ClStore *s,const ClSession *a,int id,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int i=cl_library_find(&w,2,id),result=CL_LIB_OK;
    if (i<0) result=CL_LIB_NOT_FOUND;
    else {
        ClRow *l=&w.table[2].rows[i];int reader=cl_library_number(l,2);
        if (!allowed(&w,a,reader,1)) result=CL_LIB_DENIED;
        else if (!normal_reader(&w,reader)) result=CL_LIB_BLOCKED;
        else if (!same(l,6,"active")) result=CL_LIB_CLOSED;
        else if (cl_library_overdue(l,today)) result=CL_LIB_OVERDUE;
        else if (cl_library_number(l,7)>=1) result=CL_LIB_RENEW_LIMIT;
        else {
            for (int r=0;r<w.table[4].count;++r) if (cl_library_number(&w.table[4].rows[r],1)==cl_library_number(l,1) &&
                reservation_open(&w.table[4].rows[r])) result=CL_LIB_QUEUE;
            ClDate due,next;
            if (result==CL_LIB_OK) {
                if (!cl_date_parse(l->field[4],&due) || !cl_date_add(due,15,&next)) result=CL_LIB_INVALID;
                else {cl_date_format(l->field[4],512,next);num(l,7,1);}
            }
        }
    }
    return finish(s,&w,result);
}
int cl_library_reserve(ClStore *s,const ClSession *a,int reader,int book,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int bi=cl_library_find(&w,1,book),result=CL_LIB_OK;
    if (!allowed(&w,a,reader,1)) result=CL_LIB_DENIED;
    else if (!normal_reader(&w,reader)) result=CL_LIB_BLOCKED;
    else if (bi<0) result=CL_LIB_NOT_FOUND;
    else if (!same(&w.table[1].rows[bi],6,"available")) result=CL_LIB_NO_COPY;
    else if (cl_library_number(&w.table[1].rows[bi],5)>0) result=CL_LIB_AVAILABLE;
    if (result==CL_LIB_OK) {
        for (int i=0;i<w.table[2].count;++i) {
            ClRow *l=&w.table[2].rows[i];
            if (cl_library_number(l,1)==book && cl_library_number(l,2)==reader && same(l,6,"active")) result=CL_LIB_DUPLICATE;
        }
        for (int i=0;i<w.table[4].count;++i) {
            ClRow *r=&w.table[4].rows[i];
            if (cl_library_number(r,1)==book && cl_library_number(r,2)==reader && reservation_open(r)) result=CL_LIB_DUPLICATE;
        }
        if (result==CL_LIB_OK) {
            ClRow r={0};r.count=6;int id=next_id(&w.table[4]);
            if (!id) result=CL_LIB_INVALID;
            else {
                num(&r,0,id);num(&r,1,book);num(&r,2,reader);set(&r,5,"waiting");cl_date_format(r.field[3],512,today);
                if (!cl_table_append(&w.table[4],&r)) result=CL_LIB_STORAGE;
            }
        }
    }
    return finish(s,&w,result);
}
int cl_library_cancel(ClStore *s,const ClSession *a,int id,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int ri=cl_library_find(&w,4,id),result=CL_LIB_OK;
    if (ri<0) result=CL_LIB_NOT_FOUND;
    else {
        ClRow *r=&w.table[4].rows[ri];int bi=cl_library_find(&w,1,cl_library_number(r,1));
        if (!allowed(&w,a,cl_library_number(r,2),0)) result=CL_LIB_DENIED;
        else if (!reservation_open(r)) result=CL_LIB_CLOSED;
        else if (bi<0) result=CL_LIB_NOT_FOUND;
        else {
            ClRow *b=&w.table[1].rows[bi];
            if (same(r,5,"ready")) {
                if (cl_library_number(b,5)>=cl_library_number(b,4)) result=CL_LIB_INVALID;
                else num(b,5,cl_library_number(b,5)+1);
            }
            set(r,5,"cancelled");int changed=0;
            if (result==CL_LIB_OK && !allocate(&w,cl_library_number(b,0),today,&changed)) result=CL_LIB_INVALID;
        }
    }
    return finish(s,&w,result);
}
int cl_library_pay(ClStore *s,const ClSession *a,int id,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int i=cl_library_find(&w,3,id),result=CL_LIB_OK;
    if (i<0) result=CL_LIB_NOT_FOUND;
    else {
        ClRow *f=&w.table[3].rows[i];
        if (!allowed(&w,a,cl_library_number(f,1),0)) result=CL_LIB_DENIED;
        else if (!same(f,5,"unpaid")) result=CL_LIB_CLOSED;
        else {set(f,5,"paid");cl_date_format(f->field[6],512,today);}
    }
    return finish(s,&w,result);
}
int cl_library_add_book(ClStore *s,const ClSession *a,const char *isbn,const char *title,const char *author,int copies,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int result=CL_LIB_OK;
    if (!admin(a)) result=CL_LIB_DENIED;
    else if (!isbn || !*isbn || !title || !*title || !author || !*author || copies<=0) result=CL_LIB_INVALID;
    else {
        for (int i=0;i<w.table[1].count;++i) if (same(&w.table[1].rows[i],1,isbn)) result=CL_LIB_DUPLICATE;
        if (result==CL_LIB_OK) {
            ClRow r={0};r.count=8;int id=next_id(&w.table[1]);
            if (!id || !text_copy(r.field[1],512,isbn) || !text_copy(r.field[2],512,title) ||
                !text_copy(r.field[3],512,author)) result=CL_LIB_INVALID;
            else {
                num(&r,0,id);num(&r,4,copies);num(&r,5,copies);set(&r,6,"available");cl_date_format(r.field[7],512,today);
                if (!cl_table_append(&w.table[1],&r)) result=CL_LIB_STORAGE;
                else if (!cl_store_validate(w.table)) result=CL_LIB_INVALID;
            }
        }
    }
    return finish(s,&w,result);
}
int cl_library_book_action(ClStore *s,const ClSession *a,int book,int add_copy,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int i=cl_library_find(&w,1,book),result=CL_LIB_OK;
    if (!admin(a)) result=CL_LIB_DENIED;
    else if (i<0) result=CL_LIB_NOT_FOUND;
    else {
        ClRow *b=&w.table[1].rows[i];
        if (add_copy) {
            if (cl_library_number(b,4)==CL_INT_MAX) result=CL_LIB_INVALID;
            else {num(b,4,cl_library_number(b,4)+1);num(b,5,cl_library_number(b,5)+1);}
        } else set(b,6,same(b,6,"available")?"suspended":"available");
        int changed=0;
        if (result==CL_LIB_OK && !allocate(&w,book,today,&changed)) result=CL_LIB_INVALID;
    }
    return finish(s,&w,result);
}
int cl_library_reader_action(ClStore *s,const ClSession *a,int reader,ClDate today) {
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    int i=cl_library_find(&w,0,reader),result=CL_LIB_OK;
    if (!admin(a)) result=CL_LIB_DENIED;
    else if (i<0) result=CL_LIB_NOT_FOUND;
    else if (same(&w.table[0].rows[i],7,"注销")) result=CL_LIB_CLOSED;
    else {ClRow *r=&w.table[0].rows[i];set(r,7,same(r,7,"正常")?"冻结":"正常");}
    return finish(s,&w,result);
}
int cl_library_seed(ClStore *s,const ClSession *a,ClDate today) {
    static const char *titles[]={"C程序设计语言","深入理解计算机系统","操作系统概念","算法导论",
        "计算机网络（第7版）","数据结构（C语言版）","中国文学史","时间简史",
        "自动控制原理","数字电子技术基础","现代操作系统","编译原理","人工智能：一种现代方法",
        "线性代数","高等数学","信号与系统","嵌入式系统设计","数据库系统概论","软件工程","中文长书名安全截断测试图书"};
    ClStore w={0};if (!begin(s,&w,today)) return CL_LIB_STORAGE;
    if (!admin(a) || w.table[1].count || w.table[2].count || w.table[3].count || w.table[4].count)
        return finish(s,&w,CL_LIB_DENIED);
    for (int i=0;i<20;++i) {
        ClRow r={0};r.count=8;char id[12];cl_int_format(id,12,i+1);
        num(&r,0,i+1);set(&r,1,"DEMO-");text_append(r.field[1],512,id);set(&r,2,titles[i]);
        set(&r,3,i%2?"课程参考作者":"图书馆编目组");num(&r,4,3);num(&r,5,3);
        set(&r,6,"available");cl_date_format(r.field[7],512,today);
        if (!cl_table_append(&w.table[1],&r)) return finish(s,&w,CL_LIB_STORAGE);
    }
    return finish(s,&w,CL_LIB_OK);
}
const char *cl_library_message(int result) {
    static const char *messages[]={"操作已保存。","已保存，CSV 待恢复；按 F5 刷新，勿重复提交。",
        "错误：无法读取或保存数据。","错误：提交状态待确认；按 F5 恢复后再查询。",
        "错误：没有权限，或当前账号状态已改变。","错误：记录不存在。","错误：读者冻结或未付罚款达到限制。",
        "错误：图书停借或暂无可借副本。","错误：在借图书已达 5 本。","错误：重复 ISBN、借阅或预约。",
        "错误：此记录已处理，不能重复提交。","错误：存在逾期借阅，请先归还。","错误：只能续借一次。",
        "错误：有读者预约此书，不能续借。","错误：尚有可借副本，无需预约。","错误：字段、容量或日期不合法。"};
    return result>=0 && result<=CL_LIB_INVALID?messages[result]:"错误：未知结果。";
}

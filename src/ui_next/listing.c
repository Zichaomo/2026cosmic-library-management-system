#include "ui_next/app.h"
#include <string.h>

int cl_app_loan_days(const ClRow *loan,ClDate today,int *days) {
    ClDate due;
    return loan && days && !strcmp(loan->field[6],"active") &&
        cl_date_parse(loan->field[4],&due) && cl_date_difference(due,today,days);
}
int cl_app_filter_loan(const NextApp *a,const ClRow *r) {
    int days=0,active=cl_app_loan_days(r,a->today,&days);
    switch (a->loan_filter) {
        case LOANS_ACTIVE:return active;
        case LOANS_RETURNED:return !strcmp(r->field[6],"returned");
        case LOANS_OVERDUE:return active && days<0;
        case LOANS_SOON:return active && days>=0 && days<=7;
        case LOANS_ATTENTION:return active && days<=7;
        default:return 1;
    }
}
static int before(const NextApp *a,int left,int right) {
    const ClRow *l=&a->store.table[2].rows[left],*r=&a->store.table[2].rows[right];
    int order=strcmp(l->field[4],r->field[4]);
    if (order) return a->loan_sort==1?order<0:order>0;
    return cl_library_number(l,0)<=cl_library_number(r,0);
}
void cl_app_sort_loans(NextApp *a) {
    if (a->kind!=2 || !a->loan_sort) return;
    int n=a->match_count;
    for (int width=1;width<n;width*=2) {
        for (int base=0;base<n;base+=width*2) {
            int mid=base+width,end=base+width*2;
            if (mid>n) mid=n;
            if (end>n) end=n;
            int l=base,r=mid,out=base;
            while (l<mid || r<end) {
                if (r>=end || (l<mid && before(a,a->matches[l],a->matches[r])))
                    a->sort_work[out++]=a->matches[l++];
                else a->sort_work[out++]=a->matches[r++];
            }
        }
        for (int i=0;i<n;++i) a->matches[i]=a->sort_work[i];
    }
}
void cl_app_due_counts(const NextApp *a,int *soon,int *overdue) {
    *soon=*overdue=0;
    if (!a->session.authenticated) return;
    for (int i=0;i<a->store.table[2].count;++i) {
        const ClRow *r=&a->store.table[2].rows[i];int days;
        if (a->session.role!=CL_ROLE_ADMIN && cl_library_number(r,2)!=a->session.reader_id) continue;
        if (!cl_app_loan_days(r,a->today,&days)) continue;
        if (days<0) ++*overdue;else if (days<=7) ++*soon;
    }
}
const char *cl_app_filter_name(int filter) {
    static const char *names[]={"全部","在借","已还","逾期","7天内到期","到期提醒"};
    return filter>=0 && filter<LOANS_FILTER_COUNT?names[filter]:names[0];
}
const char *cl_app_sort_name(int sort) {
    static const char *names[]={"记录顺序","应还日期升序","应还日期降序"};
    return sort>=0 && sort<3?names[sort]:names[0];
}

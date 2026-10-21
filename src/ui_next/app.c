#include "ui_next/app.h"
#include <string.h>

static void message(NextApp *a,const char *text) { text_copy(a->message,512,text); }
static int is_admin(const NextApp *a) { return a->session.authenticated && a->session.role==CL_ROLE_ADMIN; }
static int is_list(NextPage p) {
    return p==N_CATALOG || p==N_BOOKS || p==N_READERS || p==N_LOANS || p==N_MY_LOANS ||
        p==N_RESERVATIONS || p==N_MY_RESERVATIONS || p==N_FINES || p==N_MY_FINES;
}
static int is_form(NextPage p) { return p==N_LOGIN || p==N_REGISTER || p==N_NEW_BOOK || p==N_BORROW; }
static int fields(NextPage p) { return p==N_REGISTER?5:(p==N_NEW_BOOK?4:2); }
static void clear_fields(NextApp *a) {
    for (int i=0;i<5;++i) {Edit empty={{0},0};a->field[i]=empty;}
}
static int refresh(NextApp *a) {
    int result=cl_library_refresh(&a->store,a->today);
    if (result!=CL_LIB_OK && result!=CL_LIB_PENDING) {message(a,cl_library_message(result));return 0;}
    if (a->session.role==CL_ROLE_READER) {
        int ri=cl_library_find(&a->store,0,a->session.reader_id);
        if (ri<0 || strcmp(a->store.table[0].rows[ri].field[7],"正常")) {
            cl_session_clear(&a->session);clear_fields(a);a->page=N_LOGIN;
            a->command=a->discard=a->detail=a->help=a->focus=0;
            message(a,"错误：账号状态已改变，请重新登录。");return 0;
        }
    }
    return 1;
}
void cl_app_rebuild(NextApp *a) {
    a->kind=(a->page==N_READERS)?0:
        ((a->page==N_MY_LOANS || a->page==N_LOANS)?2:
         ((a->page==N_MY_FINES || a->page==N_FINES)?3:
          ((a->page==N_MY_RESERVATIONS || a->page==N_RESERVATIONS)?4:1)));
    a->match_count=0;
    for (int i=0;i<a->store.table[a->kind].count;++i) {
        const ClRow *r=&a->store.table[a->kind].rows[i];
        if (a->page==N_MY_LOANS && cl_library_number(r,2)!=a->session.reader_id) continue;
        if (a->page==N_MY_RESERVATIONS && cl_library_number(r,2)!=a->session.reader_id) continue;
        if (a->page==N_MY_FINES && cl_library_number(r,1)!=a->session.reader_id) continue;
        int found=!a->search.text[0];
        for (int c=0;!found && c<r->count;++c) {
            if (a->kind==0 && (c==3 || c==4)) continue;
            if (strstr(r->field[c],a->search.text)) found=1;
        }
        if (found) a->matches[a->match_count++]=i;
    }
    if (a->selected>=a->match_count) a->selected=a->match_count?a->match_count-1:0;
    if (a->selected<0) a->selected=0;
}
int cl_app_selected_id(const NextApp *a) {
    return a->match_count?cl_library_number(&a->store.table[a->kind].rows[a->matches[a->selected]],0):0;
}
void cl_app_enter(NextApp *a,NextPage page) {
    if (page<0 || page>=N_PAGE_COUNT) return;
    if (page>=N_ADMIN && !a->session.authenticated) {message(a,"错误：请先登录。");return;}
    if ((page==N_ADMIN || page>=N_BOOKS) && !is_admin(a)) {message(a,"错误：需要管理员权限。");return;}
    if ((page==N_READER || (page>=N_MY_LOANS && page<=N_MY_FINES)) && is_admin(a)) {
        message(a,"错误：管理员请从管理页面办理操作。");return;
    }
    a->page=page;a->focus=0;a->selected=0;a->searching=a->detail=a->dirty=a->discard=0;a->command=0;
    a->search.text[0]=0;a->search.cursor=0;clear_fields(a);
    if (page==N_NEW_BOOK) {text_copy(a->field[3].text,512,"1");a->field[3].cursor=1;}
    message(a,"");
    if (page>=N_ADMIN) refresh(a);
    cl_app_rebuild(a);
}
int cl_app_init(NextApp *a,const char *directory,ClDate today) {
    if (!a || !cl_date_valid(today)) return 0;
    NextApp empty={0};*a=empty;a->today=today;a->running=1;
    if (!cl_store_open(&a->store,directory)) return 0;
    cl_app_enter(a,N_WELCOME);return 1;
}
void cl_app_dispose(NextApp *a) {
    if (!a) return;
    cl_store_dispose(&a->store);clear_fields(a);cl_session_clear(&a->session);a->running=0;
}
static void logout(NextApp *a) {
    cl_session_clear(&a->session);cl_app_enter(a,N_WELCOME);message(a,"已退出登录；已提交的数据保留。");
}
static void result(NextApp *a,int status) { message(a,cl_library_message(status));cl_app_rebuild(a); }
static int same(Event e,char lower,char upper) { return e.key==KEY_TEXT && !e.text[1] && (e.text[0]==lower || e.text[0]==upper); }
static void request(NextApp *a,int command,int id,int reader) {
    a->command=command;a->command_id=id;a->command_reader=reader;
}
static void execute(NextApp *a) {
    int c=a->command,id=a->command_id,reader=a->command_reader,status=CL_LIB_INVALID;
    a->command=0;
    if (!refresh(a)) return;
    if (c==CMD_BORROW) status=cl_library_borrow(&a->store,&a->session,reader,id,a->today,NULL);
    if (c==CMD_RETURN) status=cl_library_return(&a->store,&a->session,id,a->today,NULL);
    if (c==CMD_RENEW) status=cl_library_renew(&a->store,&a->session,id,a->today);
    if (c==CMD_RESERVE) status=cl_library_reserve(&a->store,&a->session,reader,id,a->today);
    if (c==CMD_CANCEL) status=cl_library_cancel(&a->store,&a->session,id,a->today);
    if (c==CMD_PAY) status=cl_library_pay(&a->store,&a->session,id,a->today);
    if (c==CMD_COPY || c==CMD_SUSPEND) status=cl_library_book_action(&a->store,&a->session,id,c==CMD_COPY,a->today);
    if (c==CMD_FREEZE) status=cl_library_reader_action(&a->store,&a->session,id,a->today);
    if (a->page==N_BORROW && (status==CL_LIB_OK || status==CL_LIB_PENDING)) cl_app_enter(a,a->form_reserve?N_RESERVATIONS:N_LOANS);
    result(a,status);
}
static void submit(NextApp *a) {
    if (!refresh(a)) return;
    if (a->page==N_LOGIN) {
        int status=cl_auth_login(&a->store,a->field[0].text,a->field[1].text,&a->session);
        if (status==CL_AUTH_OK) cl_app_enter(a,is_admin(a)?N_ADMIN:N_READER);
        else {
            message(a,status==CL_AUTH_FROZEN?"错误：账号已冻结。":
                (status==CL_AUTH_STORAGE_ERROR?"错误：数据需恢复，请按 F5。":"错误：账号或密码不正确。"));
            a->focus=1;
        }
        a->field[1].text[0]=0;a->field[1].cursor=0;
    } else if (a->page==N_REGISTER) {
        ClRegistration r={a->field[0].text,a->field[1].text,a->field[2].text,a->field[3].text,a->field[4].text};
        int status=cl_auth_register(&a->store,&r);
        static const char *errors[]={"","错误：必填项为空或姓名不合法。","错误：编号须为 3～31 位字母、数字、汉字、_ 或 -。",
            "错误：请输入 11 位有效格式手机号。","错误：密码须至少 6 位且不超过 255 字节。","错误：两次输入的密码不一致。",
            "错误：账号已存在或使用了保留编号。","错误：数据保存失败。","错误：提交状态不确定，按 F5 后查询，请勿重复注册。",
            "账号已保存；请按 F5 恢复导出后登录，不要重复注册。"};
        if (status==CL_REGISTER_OK || status==CL_REGISTER_PENDING_EXPORT) {
            char number[96];text_copy(number,96,a->field[0].text);cl_app_enter(a,N_LOGIN);
            text_copy(a->field[0].text,512,number);a->field[0].cursor=(int)strlen(number);a->focus=1;
            message(a,status==CL_REGISTER_OK?"注册成功，请输入密码登录。":errors[status]);
        } else {
            message(a,status>=0 && status<=CL_REGISTER_PENDING_EXPORT?errors[status]:"错误：注册失败。");
            a->focus=status==CL_REGISTER_BAD_PHONE?4:(status==CL_REGISTER_BAD_NUMBER || status==CL_REGISTER_DUPLICATE?0:2);
        }
    } else if (a->page==N_NEW_BOOK) {
        int copies;
        if (!positive_number(a->field[3].text,&copies)) {message(a,"错误：副本数必须为正整数。");a->focus=3;return;}
        int status=cl_library_add_book(&a->store,&a->session,a->field[0].text,a->field[1].text,a->field[2].text,copies,a->today);
        if (status==CL_LIB_OK || status==CL_LIB_PENDING) cl_app_enter(a,N_BOOKS);
        result(a,status);
    } else if (a->page==N_BORROW) {
        int reader,book;
        if (!positive_number(a->field[0].text,&reader) || !positive_number(a->field[1].text,&book)) {
            message(a,"错误：请输入有效的读者 ID 和图书 ID。");return;
        }
        request(a,a->form_reserve?CMD_RESERVE:CMD_BORROW,book,reader);
    }
}
static void back(NextApp *a) {
    if (a->page==N_LOGIN || a->page==N_REGISTER) cl_app_enter(a,N_WELCOME);
    else if (a->page==N_NEW_BOOK) cl_app_enter(a,N_BOOKS);
    else if (a->page==N_ADMIN || a->page==N_READER) logout(a);
    else cl_app_enter(a,is_admin(a)?N_ADMIN:N_READER);
}
void cl_app_event(NextApp *a,Event e,ClDate today) {
    a->today=today;
    if (e.key==KEY_QUIT) {a->running=0;clear_fields(a);return;}
    if (e.key==KEY_F5) {if (refresh(a)) message(a,"数据已刷新，当前输入与选择保留。");cl_app_rebuild(a);return;}
    if (e.key==KEY_F1) {a->help=!a->help;return;}
    if (a->help) {if (e.key==KEY_ESC || e.key==KEY_ENTER) a->help=0;return;}
    if (a->command) {if (same(e,'y','Y')) execute(a);else if (same(e,'n','N') || e.key==KEY_ESC) a->command=0;return;}
    if (a->discard) {
        if (same(e,'y','Y')) {a->discard=0;back(a);}
        else if (same(e,'n','N') || e.key==KEY_ESC) a->discard=0;
        return;
    }
    if (a->detail) {if (e.key==KEY_ESC || e.key==KEY_ENTER) a->detail=0;return;}
    if (a->page==N_WELCOME || a->page==N_ADMIN || a->page==N_READER) {
        int count=a->page==N_WELCOME?3:(a->page==N_ADMIN?9:5);
        if (e.key==KEY_TAB || e.key==KEY_DOWN || e.key==KEY_RIGHT) a->focus=(a->focus+1)%count;
        else if (e.key==KEY_BACKTAB || e.key==KEY_UP || e.key==KEY_LEFT) a->focus=(a->focus+count-1)%count;
        else if (e.key==KEY_ESC) {if (a->page==N_WELCOME) a->running=0;else logout(a);}
        else if (e.key==KEY_ENTER) {
            int focus=a->focus;
            static const NextPage ap[]={N_CATALOG,N_BOOKS,N_READERS,N_LOANS,N_BORROW,N_RESERVATIONS,N_FINES,N_STATS};
            static const NextPage rp[]={N_CATALOG,N_MY_LOANS,N_MY_RESERVATIONS,N_MY_FINES};
            if (a->page==N_WELCOME) {if (focus==2) a->running=0;else cl_app_enter(a,focus?N_REGISTER:N_LOGIN);}
            else if (a->page==N_ADMIN) {if (focus==8) logout(a);else {a->form_reserve=0;cl_app_enter(a,ap[focus]);}}
            else {if (focus==4) logout(a);else cl_app_enter(a,rp[focus]);}
        }
        return;
    }
    if (a->page==N_STATS) {if (e.key==KEY_ESC || same(e,'f','F')) back(a);return;}
    if (is_list(a->page)) {
        if (a->searching) {
            if (e.key==KEY_ENTER || e.key==KEY_ESC) a->searching=0;
            else {if (e.key==KEY_TEXT) edit_insert(&a->search,e.text);else edit_key(&a->search,e.key);a->selected=0;cl_app_rebuild(a);}
            return;
        }
        int id=cl_app_selected_id(a);
        if (e.key==KEY_ESC || same(e,'f','F')) back(a);
        else if (e.key==KEY_UP && a->selected>0) --a->selected;
        else if (e.key==KEY_DOWN && a->selected+1<a->match_count) ++a->selected;
        else if (e.key==KEY_HOME) a->selected=0;
        else if (e.key==KEY_END) a->selected=a->match_count?a->match_count-1:0;
        else if (e.key==KEY_PGDN || e.key==KEY_PGUP) {
            a->selected+=e.key==KEY_PGDN?8:-8;cl_app_rebuild(a);
        } else if (same(e,'k','K')) a->searching=1;
        else if (e.key==KEY_ENTER && id) a->detail=1;
        else if (a->page==N_BOOKS && same(e,'c','C')) cl_app_enter(a,N_NEW_BOOK);
        else if ((a->page==N_CATALOG || a->page==N_BOOKS) && id && (same(e,'j','J') || same(e,'y','Y'))) {
            if (is_admin(a)) {
                a->form_reserve=same(e,'y','Y');cl_app_enter(a,N_BORROW);
                cl_int_format(a->field[1].text,512,id);a->field[1].cursor=(int)strlen(a->field[1].text);
            } else request(a,same(e,'j','J')?CMD_BORROW:CMD_RESERVE,id,a->session.reader_id);
        } else if (a->page==N_BOOKS && id && (same(e,'z','Z') || same(e,'t','T'))) request(a,same(e,'t','T')?CMD_SUSPEND:CMD_COPY,id,0);
        else if ((a->page==N_LOANS || a->page==N_MY_LOANS) && id && (same(e,'g','G') || same(e,'x','X'))) request(a,same(e,'g','G')?CMD_RETURN:CMD_RENEW,id,0);
        else if ((a->page==N_RESERVATIONS || a->page==N_MY_RESERVATIONS) && id && same(e,'q','Q')) request(a,CMD_CANCEL,id,0);
        else if ((a->page==N_FINES || a->page==N_MY_FINES) && id && same(e,'l','L')) request(a,CMD_PAY,id,0);
        else if (a->page==N_READERS && id && same(e,'d','D')) request(a,CMD_FREEZE,id,0);
        return;
    }
    if (is_form(a->page)) {
        int n=fields(a->page),controls=n+2;
        if (e.key==KEY_TAB || e.key==KEY_DOWN) a->focus=(a->focus+1)%controls;
        else if (e.key==KEY_BACKTAB || e.key==KEY_UP) a->focus=(a->focus+controls-1)%controls;
        else if (e.key==KEY_ESC) {if (a->dirty && a->page!=N_LOGIN) a->discard=1;else back(a);}
        else if (e.key==KEY_ENTER) {
            if (a->focus<n) ++a->focus;
            else if (a->focus==n) submit(a);
            else {if (a->dirty && a->page!=N_LOGIN) a->discard=1;else back(a);}
        } else if (a->focus<n) {
            int secret=(a->page==N_LOGIN && a->focus==1) || (a->page==N_REGISTER && (a->focus==2 || a->focus==3));
            int changed=e.key==KEY_TEXT?(secret?edit_insert_password(&a->field[a->focus],e.text):
                edit_insert(&a->field[a->focus],e.text)):edit_key(&a->field[a->focus],e.key);
            if (changed) a->dirty=1;
        }
    }
}
const char *cl_app_page_title(NextPage p) {
    static const char *titles[]={"欢迎","账号登录","读者注册","管理员工作台","读者工作台","图书目录",
        "我的借阅","我的预约","我的罚款","图书管理","新增图书","读者管理","借阅管理","办理借书 / 预约",
        "预约管理","罚款管理","统计报表"};
    return p>=0 && p<N_PAGE_COUNT?titles[p]:"未知页面";
}

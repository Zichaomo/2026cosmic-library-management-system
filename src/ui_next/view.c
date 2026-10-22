#include "ui_next/app.h"
#include <string.h>

static void put(int x,int y,const char *s,int color,int width) {cl_ui_clipped(x,y,s,color,width);}
static void centered(int y,const char *s,int color) {put((CL_SCREEN_W-utf8_width(s))/2,y,s,color,CL_SCREEN_W);}
static void label(int x,int y,const char *s) {put(x,y,s,MUTED,24);}
static void long_number(int x,int y,long long n,int width) {
    char reverse[24],text[24];int count=0,used=0;
    if (n<0) n=0;
    do {reverse[count++]=(char)('0'+n%10);n/=10;} while (n);
    while (count) text[used++]=reverse[--count];
    text[used]=0;
    put(x,y,text,TEXT,width);
}
static const ClRow *record(const NextApp *a,int kind,int id) {
    int i=cl_library_find(&a->store,kind,id);return i<0?NULL:&a->store.table[kind].rows[i];
}
static const char *account(const NextApp *a,int id) {
    const ClRow *r=record(a,0,id);return r?r->field[1]:"记录不可用";
}
static const char *book_title(const NextApp *a,int id) {
    const ClRow *b=record(a,1,id);return b?b->field[2]:"记录不可用";
}
static void command_records(const NextApp *a,int *book,int *reader) {
    *book=*reader=0;const ClRow *r=NULL;
    if (a->command==CMD_BORROW || a->command==CMD_RESERVE) {*book=a->command_id;*reader=a->command_reader;}
    else if (a->command==CMD_COPY || a->command==CMD_SUSPEND) *book=a->command_id;
    else if (a->command==CMD_FREEZE) *reader=a->command_id;
    else if (a->command==CMD_RETURN || a->command==CMD_RENEW) r=record(a,2,a->command_id);
    else if (a->command==CMD_CANCEL) r=record(a,4,a->command_id);
    else if (a->command==CMD_PAY) {
        const ClRow *f=record(a,3,a->command_id);
        if (f) {*reader=cl_library_number(f,1);r=record(a,2,cl_library_number(f,2));}
    }
    if (r) {*book=cl_library_number(r,1);*reader=cl_library_number(r,2);}
}
static void heading(const NextApp *a) {
    cl_ui_fill(0,0,120,35,BG);put(4,1,"宇宙图书馆  /  COSMIC LIBRARY",INK,70);
    put(92,1,"BUCT · 图书管理",MUTED,24);cl_ui_line(4,2,112);cl_ui_line(4,32,112);
    put(4,4,a->page==N_NEW_BOOK && a->edit_book_id?"编辑图书":cl_app_page_title(a->page),INK,75);
    cl_ui_fill(0,34,120,1,STONE);
    if (a->session.authenticated) {
        put(4,34,a->session.role==CL_ROLE_ADMIN?"管理员":"读者",MUTED,8);
        put(13,34,a->session.username,MUTED,40);
    } else put(4,34,"未登录",MUTED,40);
    put(60,34,"120 x 35  |  单进程",MUTED,29);
    put(92,34,a->store.blocked?"状态待确认":(a->store.pending_export?"已保存待导出":
        (a->store.recovered?"已恢复，检查数据":"CSV 已连接")),a->store.blocked || a->store.recovered?ERROR_COLOR:MUTED,24);
    put(4,33,"Tab / 方向键  Enter 确认  F5 刷新  Esc 返回  F1 帮助  Ctrl+Q 退出",MUTED,112);
}
static void home(const NextApp *a) {
    if (a->page==N_WELCOME) {
        centered(7,"宇宙图书馆管理系统",INK);centered(9,"让每一次借阅，都有清晰的去向。",MUTED);
        const char *items[]={"账 号 登 录","读 者 注 册","退 出 程 序"};
        for (int i=0;i<3;++i) cl_ui_button(51,13+i*5,items[i],a->focus==i);
        centered(29,"图书、借阅和账号修改会保存在当前数据目录。",MUTED);
    } else {
        static const char *admin[]={"图书目录","图书管理","读者管理","借阅管理","办理借书","预约管理","罚款管理","统计报表","退出登录"};
        static const char *reader[]={"图书目录","我的借阅","我的预约","我的罚款","退出登录"};
        static const char *admin_hint[]={"查找馆藏，查看可借副本与图书详情。","新增图书、增加副本，管理停借状态。","查看读者资料，办理冻结与解冻。","查看借阅记录，办理归还与续借。","为读者办理借书，提交前核对可借库存。","查看预约队列与待取记录，办理取消。","查看罚款明细，登记模拟缴费。","汇总馆藏、借阅、预约与罚款数据。","返回欢迎页，已提交的数据继续保留。"};
        static const char *reader_hint[]={"按书名、作者或 ISBN 查找下一本想读的书。","查看应还日期，办理归还或续借。","查看排队与待取状态，取消不需要的预约。","查看罚款明细，办理模拟缴费。","返回欢迎页，已提交的数据继续保留。"};
        int admin_mode=a->page==N_ADMIN,count=admin_mode?9:5;
        int active=0,ready=0;long long available=0,unpaid=0;
        for (int i=0;i<a->store.table[1].count;++i) available+=cl_library_number(&a->store.table[1].rows[i],5);
        for (int i=0;i<a->store.table[2].count;++i) {
            const ClRow *r=&a->store.table[2].rows[i];
            if ((admin_mode || cl_library_number(r,2)==a->session.reader_id) && !strcmp(r->field[6],"active")) ++active;
        }
        for (int i=0;i<a->store.table[4].count;++i) {
            const ClRow *r=&a->store.table[4].rows[i];
            if ((admin_mode || cl_library_number(r,2)==a->session.reader_id) && !strcmp(r->field[5],"ready")) ++ready;
        }
        for (int i=0;i<a->store.table[3].count;++i) {
            const ClRow *r=&a->store.table[3].rows[i];
            if ((admin_mode || cl_library_number(r,1)==a->session.reader_id) && !strcmp(r->field[5],"unpaid")) unpaid+=cl_library_number(r,3);
        }
        cl_ui_fill(4,7,29,23,STONE);put(7,8,admin_mode?"馆务导航":"我的图书馆",MUTED,23);
        for (int i=0;i<count;++i) {
            int y=i==count-1?28:10+i*2,selected=a->focus==i;
            if (selected) cl_ui_fill(6,y,25,1,SELECTED);
            put(8,y,selected?">":" ",ACCENT,1);
            put(10,y,admin_mode?admin[i]:reader[i],selected?ACCENT:TEXT,19);
        }
        put(38,7,admin_mode?"馆务概览":"阅读近况",INK,72);
        put(38,8,admin_mode?"从日常办理开始，让每一本书流转有序。":"让每一次借阅，都有清晰的去向。",MUTED,76);
        int soon,overdue;cl_app_due_counts(a,&soon,&overdue);
        put(38,9,"7天内到期",MUTED,12);cl_ui_integer(50,9,soon,ACCENT);
        put(58,9,"已逾期",MUTED,8);cl_ui_integer(66,9,overdue,overdue?ERROR_COLOR:ACCENT);
        put(77,9,"D 查看到期提醒",ACCENT,36);
        const char *metrics_admin[]={"可借册数","在借记录","待取预约"};
        const char *metrics_reader[]={"我的在借","待取预约","未付罚款 / 分"};
        long long values_admin[]={available,active,ready},values_reader[]={active,ready,unpaid};
        for (int i=0;i<3;++i) {
            int x=38+i*27;cl_ui_fill(x,11,24,5,PAPER);
            put(x+2,12,admin_mode?metrics_admin[i]:metrics_reader[i],MUTED,20);
            long_number(x+2,14,admin_mode?values_admin[i]:values_reader[i],20);
        }
        cl_ui_fill(38,18,78,12,PAPER);
        put(41,19,"当前事项",MUTED,70);
        put(41,21,admin_mode?admin[a->focus]:reader[a->focus],INK,70);
        put(41,23,admin_mode?admin_hint[a->focus]:reader_hint[a->focus],TEXT,70);
        cl_ui_line(41,25,72);
        put(41,27,"Enter 进入    上 / 下选择    Tab 切换",ACCENT,70);
    }
}
static void forms(const NextApp *a) {
    if (a->page==N_LOGIN || a->page==N_BORROW) {
        int login=a->page==N_LOGIN;
        if (login) {centered(6,"宇宙图书馆管理系统",INK);centered(8,"让每一次借阅，都有清晰的去向。",MUTED);}
        cl_ui_box(24,10,72,18,login?"账号登录":(a->form_reserve?"为读者办理预约":"为读者办理借书"));
        for (int i=0;i<2;++i) {
            put(29,14+i*4,a->focus==i?">":" ",ACCENT,1);
            put(31,14+i*4,login?(i?"密码":"账号"):(i?"书名 / ISBN":"读者 / 账号"),a->focus==i?ACCENT:TEXT,12);
            cl_ui_input(43,13+i*4,45,&a->field[i],a->focus==i,login && i==1);
        }
        if (!login) {
            const ClRow *person=record(a,0,a->borrow_reader_id),*title=record(a,1,a->borrow_book_id);
            if (person) {put(43,16,"姓名：",MUTED,6);put(50,16,person->field[2],TEXT,38);}
            if (title) {put(43,20,"ISBN：",MUTED,6);put(50,20,title->field[1],TEXT,38);}
        }
        cl_ui_button(43,22,login?"登 录":(a->form_reserve?"预 约":"借 出"),a->focus==2);
        cl_ui_button(66,22,"返 回",a->focus==3);
        put(29,26,login?"输入后 Enter 下一项；选择登录后 Enter 确认。":"支持中文、拼音、账号；多个结果可直接选择。",MUTED,60);
        if (login) put(31,29,"尚无账号？返回欢迎页选择读者注册。",MUTED,72);
    } else {
        int reg=a->page==N_REGISTER,n=reg?5:(a->edit_book_id?3:4);
        static const char *rl[]={"读者编号","姓名","密码","确认密码","手机号"};
        static const char *bl[]={"ISBN","书名","作者","副本数"};
        cl_ui_box(20,5,80,26,reg?"读者注册":(a->edit_book_id?"编辑图书":"新增图书"));
        for (int i=0;i<n;++i) {
            int y=reg?7+i*4:9+i*4;
            put(25,y+1,a->focus==i?">":" ",ACCENT,1);
            put(27,y+1,reg?rl[i]:bl[i],a->focus==i?ACCENT:TEXT,14);
            cl_ui_input(44,y,48,&a->field[i],a->focus==i,reg && (i==2 || i==3));
        }
        if (a->edit_book_id) put(25,23,"仅修改资料；库存请用“增加副本”办理。",MUTED,68);
        cl_ui_button(44,27,reg?"注 册":"保 存",a->focus==n);cl_ui_button(70,27,"取 消",a->focus==n+1);
    }
    put(4,33,"左右 定位光标  Home/End 首尾  直接输入 插入  Backspace/Delete 删除  Tab 切换",MUTED,112);
}
static void row(const NextApp *a,const ClRow *r,int y,int active) {
    int fg=active?PRIMARY_TEXT:TEXT;
    if (active) {cl_ui_fill(5,y-1,110,2,SELECTED);put(7,y,">",ACCENT,1);}
    if (a->kind==1) {
        put(10,y,r->field[2],fg,38);put(52,y,r->field[3],fg,31);
        put(86,y,r->field[5],fg,10);put(97,y,"/",MUTED,1);put(99,y,r->field[4],fg,9);
        put(109,y,!strcmp(r->field[6],"suspended")?"停借":(cl_library_number(r,5)>0?"可借":"缺书"),MUTED,4);
    } else {
        put(10,y,r->field[0],fg,9);
        if (a->kind==0) {
            put(22,y,r->field[1],fg,23);put(48,y,r->field[2],fg,28);put(80,y,r->field[5],fg,22);put(105,y,r->field[7],MUTED,8);
        } else if (a->kind==2) {
            put(20,y,account(a,cl_library_number(r,2)),fg,16);put(38,y,book_title(a,cl_library_number(r,1)),fg,29);
            put(70,y,r->field[4],fg,11);put(84,y,!strcmp(r->field[6],"active")?"在借":"已还",fg,6);
            put(94,y,r->field[7],fg,4);cl_ui_integer(103,y,cl_library_overdue(r,a->today),fg);
        } else if (a->kind==4) {
            put(20,y,account(a,cl_library_number(r,2)),fg,16);put(38,y,book_title(a,cl_library_number(r,1)),fg,25);
            put(65,y,r->field[3],fg,12);put(80,y,r->field[4],fg,12);
            const char *state=!strcmp(r->field[5],"waiting")?"排队中":(!strcmp(r->field[5],"ready")?"待取":
                (!strcmp(r->field[5],"fulfilled")?"已取":(!strcmp(r->field[5],"expired")?"已超时":"已取消")));
            put(95,y,state,MUTED,18);
        } else {
            put(22,y,r->field[1],fg,10);put(36,y,r->field[2],fg,10);put(50,y,r->field[3],fg,12);put(65,y,r->field[4],fg,14);
            put(83,y,!strcmp(r->field[5],"paid")?"已缴":"未缴",fg,8);put(96,y,r->field[6],fg,16);
        }
    }
}
static void lists(const NextApp *a) {
    put(4,6,"检索",TEXT,8);cl_ui_input(12,5,72,&a->search,a->searching,0);
    if (!*a->search.text && !a->searching) put(15,6,a->kind==1?"书名 / 作者 / ISBN / 拼音 / 首字母":(a->kind==0?"编号 / 姓名 / 拼音 / 状态":"书名 / 读者 / 拼音 / 编号 / 日期"),MUTED,60);
    put(90,6,a->searching?"Enter 完成检索":"按 K 键进入检索",MUTED,25);
    cl_ui_integer(88,4,a->match_count,MUTED);put(98,4,"条",MUTED,3);
    cl_ui_integer(103,4,a->match_count?a->selected/8+1:0,MUTED);put(107,4,"/",MUTED,1);
    cl_ui_integer(110,4,(a->match_count+7)/8,MUTED);
    if (a->kind==2) {
        put(5,8,"S 筛选：",MUTED,10);put(15,8,cl_app_filter_name(a->loan_filter),ACCENT,24);
        put(45,8,"P 排序：",MUTED,10);put(55,8,cl_app_sort_name(a->loan_sort),ACCENT,30);
    }
    cl_ui_box(4,9,112,20,cl_app_page_title(a->page));
    if (a->kind==1) {label(10,11,"书名");label(52,11,"作者");label(86,11,"可借/总数");put(109,11,"状态",MUTED,4);}
    else {
        label(10,11,"ID");if (a->kind==0 || a->kind==3) label(22,11,a->kind==0?"读者编号":"读者 ID");
        if (a->kind==0) {label(48,11,"姓名");label(80,11,"手机号");put(105,11,"状态",MUTED,8);}
        else if (a->kind==2) {label(20,11,"读者账号");label(38,11,"书名");label(70,11,"应还日期");put(84,11,"状态",MUTED,6);put(94,11,"续借",MUTED,6);put(103,11,"逾期天",MUTED,8);}
        else if (a->kind==4) {label(20,11,"读者账号");label(38,11,"书名");label(65,11,"预约日期");label(80,11,"保留至");put(95,11,"状态",MUTED,16);}
        else {label(36,11,"借阅 ID");label(50,11,"金额/分");label(65,11,"生成日期");label(83,11,"状态");put(96,11,"缴费日期",MUTED,16);}
    }
    int page=a->selected/8;
    for (int i=0;i<8;++i) {
        int index=page*8+i;if (index>=a->match_count) break;
        row(a,&a->store.table[a->kind].rows[a->matches[index]],13+i*2,index==a->selected);
    }
    if (a->match_count>8) {
        put(114,12,"^",MUTED,1);put(114,28,"v",MUTED,1);
        cl_ui_scrollbar(114,13,15,a->selected,a->match_count,8);
    }
    if (!a->match_count) put(12,18,"暂无记录或没有匹配结果。",MUTED,96);
    else {
        const ClRow *r=&a->store.table[a->kind].rows[a->matches[a->selected]];
        put(5,29,"选中 ID：",MUTED,10);put(16,29,r->field[0],MUTED,12);
        if (a->kind==1 || a->kind==0) put(30,29,r->field[2],MUTED,82);
        else {
            int bi=a->kind==3?-1:cl_library_find(&a->store,1,cl_library_number(r,1));
            if (bi>=0) put(30,29,a->store.table[1].rows[bi].field[2],MUTED,82);
        }
    }
    const char *actions=a->kind==0?"D 冻结/解冻  Enter 详情":
        (a->kind==2?"G 归还  X 续借  S 筛选  P 排序  Enter 详情":(a->kind==3?"L 模拟缴费  Enter 详情":
        (a->kind==4?"Q 取消预约  Enter 详情":(a->page==N_BOOKS?"C 新增  B 编辑  Z 增加副本  T 停借/恢复  J 借书  Y 预约":
        "J 借书/办理借书  Y 预约/办理预约  Enter 详情"))));
    put(5,30,actions,ACCENT,110);
    put(4,33,"上/下 选择  PgUp/PgDn 翻页  K 检索  F 返回工作台  F5 刷新  Esc 返回  Ctrl+Q 退出",MUTED,112);
}
static void stats(const NextApp *a) {
    long long copies=0,available=0,unpaid=0,paid=0;int active=0,ready=0,waiting=0;
    for (int i=0;i<a->store.table[1].count;++i) {copies+=cl_library_number(&a->store.table[1].rows[i],4);available+=cl_library_number(&a->store.table[1].rows[i],5);}
    for (int i=0;i<a->store.table[2].count;++i) if (!strcmp(a->store.table[2].rows[i].field[6],"active")) ++active;
    for (int i=0;i<a->store.table[3].count;++i) {
        const ClRow *r=&a->store.table[3].rows[i];
        if (!strcmp(r->field[5],"paid")) paid+=cl_library_number(r,3);else unpaid+=cl_library_number(r,3);
    }
    for (int i=0;i<a->store.table[4].count;++i) {
        const char *s=a->store.table[4].rows[i].field[5];if (!strcmp(s,"ready")) ++ready;else if (!strcmp(s,"waiting")) ++waiting;
    }
    const char *labels[]={"图书种数","库存总册数","可借册数","读者人数","在借记录数","排队预约数","待取预约数","未付罚款/分","已缴罚款/分"};
    long long values[]={a->store.table[1].count,copies,available,a->store.table[0].count,active,waiting,ready,unpaid,paid};
    cl_ui_box(14,7,92,22,"当前数据汇总");
    for (int i=0;i<9;++i) {int x=i%2?65:20,y=10+(i/2)*3;label(x,y,labels[i]);long_number(x+19,y,values[i],18);}
}
void cl_app_scene(const NextApp *a) {
    heading(a);
    if (a->page==N_WELCOME || a->page==N_ADMIN || a->page==N_READER) home(a);
    else if (a->page==N_LOGIN || a->page==N_REGISTER || a->page==N_NEW_BOOK || a->page==N_BORROW) forms(a);
    else if (a->page==N_STATS) stats(a);
    else lists(a);
    put(4,31,a->message,strstr(a->message,"错误：")==a->message?ERROR_COLOR:MUTED,112);
    if (a->help) {
        cl_ui_box(18,7,84,22,"帮助与保存状态");
        put(23,10,"方向键选择，Tab 切换；输入框 Enter 下一项。",TEXT,72);
        put(23,12,"K 检索  J 借书  Y 预约  G 归还  X 续借",TEXT,72);
        put(23,14,"Q 取消预约  L 模拟缴费  F 返回工作台",TEXT,72);
        put(23,16,"C 新增  B 编辑  Z 增加副本  T 停借/恢复",TEXT,72);
        put(23,18,"D 工作台提醒/读者冻结；借阅 S 筛选 P 排序。",TEXT,72);
        put(23,20,"已保存待导出：勿重复提交，F5 恢复后查询。",TEXT,72);
        put(23,22,"已恢复：可能回退上一代数据，请核对记录。",ERROR_COLOR,72);
        put(23,24,"状态待确认：F5 恢复；失败时退出并保留文件。",TEXT,72);
        put(23,26,"缩放后按 F5 恢复；Esc / Enter 关闭帮助。",ACCENT,72);
    } else if (a->picker) {
        cl_ui_box(14,5,92,27,a->picker==1?"选择读者":"选择图书");
        put(19,8,"多个匹配结果，请核对姓名、账号或 ISBN。",MUTED,80);
        int page=a->picker_selected/6;
        for (int i=0;i<6;++i) {
            int index=page*6+i;if (index>=a->picker_count) break;
            int y=10+i*3,active=index==a->picker_selected;
            const ClRow *r=record(a,a->picker==1?0:1,a->picker_ids[index]);
            if (active) cl_ui_fill(16,y-1,88,3,SELECTED);
            put(18,y,active?">":" ",ACCENT,1);
            put(20,y,r?r->field[2]:"记录已不可用",active?ACCENT:TEXT,80);
            if (r && a->picker==1) {put(20,y+1,"账号：",MUTED,6);put(27,y+1,r->field[1],TEXT,72);}
            if (r && a->picker==2) {put(20,y+1,r->field[3],MUTED,30);put(53,y+1,"ISBN",MUTED,5);put(59,y+1,r->field[1],MUTED,42);}
        }
        put(19,29,"上/下 选择  PgUp/PgDn 翻页  Enter 确认  Esc 返回",ACCENT,80);
    } else if (a->command || a->discard) {
        static const char *commands[]={"","借书","归还","续借","预约","取消预约","模拟缴费","增加副本","停借/恢复","冻结/解冻"};
        cl_ui_box(20,8,80,21,a->discard?"放弃未提交的输入？":"确认操作");
        put(25,10,a->discard?"未提交的表单输入将丢弃。":commands[a->command],INK,70);
        if (a->command) {
            int book,reader;command_records(a,&book,&reader);
            const ClRow *b=record(a,1,book),*r=record(a,0,reader);
            if (book) {
                label(25,13,"图书");put(37,13,b?b->field[2]:"记录已不可用",INK,58);
                label(25,15,"ISBN");put(37,15,b?b->field[1]:"",MUTED,58);
            }
            if (reader) {
                label(25,18,"读者");put(37,18,r?r->field[2]:"记录已不可用",INK,58);
                label(25,20,"账号");put(33,20,r?r->field[1]:"",TEXT,62);
            }
        }
        put(25,26,"Y 确认    N / Esc 返回",ACCENT,70);
    } else if (a->detail && a->match_count) {
        const ClRow *r=&a->store.table[a->kind].rows[a->matches[a->selected]];
        static const char *names[5][9]={{"ID","编号","姓名","密码散列","盐","手机号","邮箱","状态","创建时间"},
            {"ID","ISBN","书名","作者","总数","可借","状态","创建日期"},
            {"ID","图书 ID","读者 ID","借出日期","应还日期","归还日期","状态","续借次数"},
            {"ID","读者 ID","借阅 ID","金额/分","生成日期","状态","缴费日期"},
            {"ID","图书 ID","读者 ID","预约日期","保留至","状态"}};
        cl_ui_box(13,7,94,23,"记录详情");int line=0;
        for (int i=0;i<r->count;++i) {
            if (a->kind==0 && (i==3 || i==4)) continue;
            label(18,10+line*2,names[a->kind][i]);put(38,10+line*2,r->field[i],TEXT,64);++line;
        }
        put(18,27,"Esc / Enter 返回列表",ACCENT,84);
    }
}

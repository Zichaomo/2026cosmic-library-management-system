#include "preview.h"
#include <string.h>
#define fill cl_ui_fill
#define put cl_ui_put
#define clipped cl_ui_clipped
#define integer_at cl_ui_integer
#define line cl_ui_line
#define box cl_ui_box
#define input cl_ui_input
#define button cl_ui_button

static int is_error(const Preview *p) { return strstr(p->message,"错误：")==p->message; }

static void draw_login(const Preview *p) {
    put(44,6,"宇宙图书馆管理系统",INK,40);
    put(40,8,"让每一次借阅，都有清晰的去向。",MUTED,60);
    box(24,10,72,18,"账号登录");
    put(29,14,p->focus==0?"> 账号":"  账号",p->focus==0?ACCENT:TEXT,12);
    put(29,18,p->focus==1?"> 密码":"  密码",p->focus==1?ACCENT:TEXT,12);
    input(43,13,45,&p->login[0],p->focus==0,0);
    input(43,17,45,&p->login[1],p->focus==1,1);
    button(43,22,"登 录",p->focus==2); button(66,22,"返 回",p->focus==3);
    clipped(29,26,p->message,is_error(p)?ERROR_COLOR:MUTED,60);
    put(35,29,"演示账号：admin / admin123",MUTED,64);
    put(4,33,"Tab 切换  Enter 下一项/确认  F5 刷新  Esc 退出  F1 帮助  Ctrl+Q 退出",MUTED,112);
}

static void draw_books(const Preview *p) {
    int page = p->match_count ? p->selected/PAGE_ROWS : 0;
    integer_at(83,4,p->match_count,MUTED); put(87,4,"种图书  |  第",MUTED,18);
    integer_at(102,4,p->match_count?page+1:0,MUTED); put(104,4,"/",MUTED,1);
    integer_at(107,4,(p->match_count+PAGE_ROWS-1)/PAGE_ROWS,MUTED); put(110,4,"页",MUTED,2);
    put(4,6,"检索",TEXT,8); input(12,5,72,&p->search,p->searching,0);
    if (!p->search.text[0] && !p->searching) put(15,6,"输入书名 / 作者 / ISBN",MUTED,60);
    put(90,6,p->searching?"Enter 完成  Esc 退出":"按 K 键进入检索",MUTED,28);
    box(4,9,112,21,"馆藏列表");
    put(10,11,"书名",MUTED,38); put(52,11,"作者",MUTED,31);
    put(86,11,"可借/总数",MUTED,16); put(109,11,"状态",MUTED,4);
    if (!p->match_count) put(12,18,"没有匹配结果；按 / 修改或删除检索词。",MUTED,96);
    for (int row=0;row<PAGE_ROWS;++row) {
        int index = page*PAGE_ROWS+row, y=13+row*2;
        if (index>=p->match_count) break;
        const PreviewBook *b=&p->books[p->matches[index]];
        int selected=index==p->selected;
        if (selected) { fill(5,y-1,110,2,SELECTED); put(7,y,">",ACCENT,1); }
        clipped(10,y,b->title,selected?PRIMARY_TEXT:TEXT,38);
        clipped(52,y,b->author,selected?PRIMARY_TEXT:TEXT,31);
        integer_at(86,y,b->available,TEXT); put(97,y,"/",MUTED,1); integer_at(99,y,b->copies,TEXT);
        put(109,y,b->suspended?"停借":(b->available?"可借":"缺书"),MUTED,4);
    }
    if (p->match_count) {
        const PreviewBook *b=&p->books[p->matches[p->selected]];
        put(5,30,"选中：",MUTED,6); clipped(11,30,b->title,MUTED,50);
        put(63,30,"ISBN",MUTED,5); clipped(69,30,b->isbn,MUTED,46);
    }
    clipped(32,34,p->message,MUTED,58);
    put(5,31,"C 新增图书    Z 增加副本    T 停借/恢复    Enter 查看详情",ACCENT,110);
    put(4,33,"上/下 选择  PgUp/PgDn 翻页  K 检索  F5 刷新  Esc 退出登录  F1 帮助",MUTED,112);
}

static void draw_form(const Preview *p) {
    static const char *labels[]={"ISBN","书名","作者","副本数"};
    box(14,7,92,24,"图书信息");
    for (int i=0;i<4;++i) {
        int y=10+i*4;
        put(20,y+1,p->focus==i?">":" ",p->focus==i?ACCENT:TEXT,1);
        put(22,y+1,labels[i],p->focus==i?ACCENT:TEXT,12);
        input(36,y,61,&p->form[i],p->focus==i,0);
    }
    clipped(36,25,p->message,is_error(p)?ERROR_COLOR:MUTED,65);
    button(36,27,"保 存",p->focus==4); button(60,27,"取 消",p->focus==5);
    put(17,31,"原型仅写入内存；退出后样例修改消失。",MUTED,92);
    put(4,33,"Tab 下一项  Enter 下一项/确认  F5 刷新  Esc 返回  F1 帮助  Ctrl+Q 退出",MUTED,112);
}

void preview_scene(const Preview *p) {
    fill(0,0,PREVIEW_W,PREVIEW_H,BG);
    put(4,1,"宇宙图书馆  /  COSMIC LIBRARY",INK,70); put(92,1,"BUCT · 交互原型",MUTED,26);
    line(4,2,112); line(4,32,112);
    put(4,4,p->page==PAGE_LOGIN?"欢迎  /  账号登录":
        (p->page==PAGE_FORM?"管理员  /  图书管理  /  新增图书":"管理员  /  图书管理"),INK,75);
    fill(0,34,PREVIEW_W,1,STONE);
    put(4,34,p->page==PAGE_LOGIN?"未登录  |  内存样例":"管理员 admin  |  内存样例",MUTED,60);
    put(92,34,"UTF-8  |  120 x 35",MUTED,27);
    if (p->page==PAGE_LOGIN) draw_login(p);
    else if (p->page==PAGE_BOOKS) draw_books(p);
    else draw_form(p);
    if (p->help) {
        box(18,8,84,19,"原型帮助");
        put(23,11,"这里只验证界面与输入，不连接正式认证或 CSV。",TEXT,72);
        put(23,14,"方向键与翻页键：移动列表 / 编辑光标",TEXT,72);
        put(23,16,"Home / End：到开头 / 末尾；Delete：删除右侧字符",TEXT,72);
        put(23,18,"中文输入要求启动前设置代码页 65001。",TEXT,72);
        put(23,20,"当前仅保证普通中文和 ASCII；组合符与 Emoji 暂不支持。",TEXT,72);
        put(23,22,"Ctrl+Q 随时退出；Esc / Enter 关闭帮助。",ACCENT,72);
        put(23,24,"窗口放大后按 F5 刷新；按键后完整补画。",MUTED,72);
    } else if (p->confirm_cancel) {
        box(25,12,70,11,"放弃未保存内容？");
        put(30,15,"Y 放弃并返回    N / Esc 继续编辑",ACCENT,58);
        put(30,19,"此操作只影响原型内存中的未保存输入。",MUTED,58);
    } else if (p->detail && p->match_count) {
        const PreviewBook *b=&p->books[p->matches[p->selected]];
        box(13,9,94,17,"图书详情（内存样例）");
        clipped(18,12,b->title,INK,84); clipped(18,15,b->author,TEXT,84);
        clipped(18,18,b->isbn,MUTED,84); put(18,22,"Esc / Enter 返回列表",ACCENT,84);
    }
}


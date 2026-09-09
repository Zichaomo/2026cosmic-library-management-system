#ifndef LMS_UI_SCREEN_H
#define LMS_UI_SCREEN_H

#include "../types.h"
#include "input.h"

/* ============ 页面路由与布局（FR-205/FR-206，PRD §4.3 导航地图） ============ */

typedef enum {
    SCREEN_WELCOME,          /* 欢迎页：登录/注册/帮助/退出 */
    SCREEN_LOGIN,            /* 登录表单 */
    SCREEN_REGISTER,         /* 注册读者表单 */
    SCREEN_ADMIN_MENU,       /* 管理员主菜单 */
    SCREEN_BOOK_LIST,        /* 图书列表/检索 */
    SCREEN_BOOK_EDIT,        /* 图书新增/编辑 */
    SCREEN_READER_LIST,      /* 读者管理 */
    SCREEN_LOAN_MANAGE,      /* 借阅管理：借书/还书/续借/逾期 */
    SCREEN_RESERVATION_MANAGE, /* 预约管理 */
    SCREEN_FINE_MANAGE,      /* 罚款管理 */
    SCREEN_STATS,            /* 统计报表 */
    SCREEN_READER_CENTER,    /* 读者个人中心 */
    SCREEN_HELP,             /* 帮助 */
    SCREEN_DICT_MANAGE,      /* 字典管理（作者/出版社/分类） */
    SCREEN_CHANGE_PWD,       /* 修改密码 */
    SCREEN_COUNT
} ScreenId;

/* 切换页面（进入时触发该页布局/绘制） */
Result screen_goto(ScreenId id);

/* 绘制当前页面 */
void screen_draw(void);

/* 分发输入事件到当前页面，返回是否已消费 */
bool screen_handle(const InputEvent *ev);

/* 返回上一页（Esc / 关闭按钮） */
void screen_back(void);

/* 在内容区显示一条临时消息（下个操作后清除） */
void screen_message(const char *text);

/* 应用退出标志（Ctrl+Q 置位，主循环据此退出） */
extern bool g_app_quit;

#endif /* LMS_UI_SCREEN_H */

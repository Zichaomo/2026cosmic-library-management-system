/* 页面路由与布局 - 界面层（M1：欢迎/登录/注册/主菜单/帮助） */
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "console.h"
#include "theme.h"
#include "widget.h"
#include "../core/auth.h"
#include "../utils/date.h"
#include "../utils/string.h"

bool g_app_quit = false;

static ScreenId s_current = SCREEN_WELCOME;
static ScreenId s_history[32];
static int s_history_len = 0;
static int s_cols = 100, s_rows = 30;

static char s_msg[128]; /* 内容区临时消息 */

/* ---------- 表单状态 ---------- */
typedef struct {
    char account[64];
    char password[64];
    int focus; /* 0=账号 1=密码 */
} LoginForm;

typedef struct {
    char name[64];
    char reader_no[32];
    char password[64];
    char confirm[64];
    char phone[32];
    char email[64];
    int focus; /* 0..5 */
} RegForm;

static LoginForm s_login;
static RegForm s_reg;

/* ---------- 静态控件 ---------- */
static Widget *g_welcome_btns[4];
static Widget *g_login_btns[2];
static Widget *g_reg_btns[2];
static Widget *g_admin_list;
static Widget *g_reader_list;
static Widget *g_help_btn;

static const char *ADMIN_ITEMS[] = {
    "1. 图书管理", "2. 字典管理", "3. 读者管理",
    "4. 借阅管理", "5. 预约管理", "6. 罚款管理",
    "7. 统计报表", "8. 退出登录"
};
static const char *READER_ITEMS[] = {
    "1. 图书检索与详情", "2. 我的借阅", "3. 我的预约",
    "4. 我的罚款", "5. 退出登录"
};

static const char *SCREEN_NAMES[SCREEN_COUNT] = {
    "欢迎", "登录", "注册", "管理员主菜单", "图书列表", "图书编辑",
    "读者管理", "借阅管理", "预约管理", "罚款管理", "统计报表",
    "读者个人中心", "帮助"
};

/* ---------- 前置声明 ---------- */
static void welcome_action(int idx);
static void do_login(void);
static void do_register(void);
static void admin_action(int idx);
static void reader_action(int idx);
static void jump_to(ScreenId id);

/* ---------- 小工具 ---------- */
static void utf8_backspace(char *s) {
    size_t len = strlen(s);
    char *p;
    if (len == 0) return;
    p = s + len - 1;
    while (p > s && ((unsigned char)*p & 0xC0) == 0x80) p--; /* 跳过 UTF-8 续字节 */
    *p = '\0';
}

static void field_append(char *buf, size_t cap, wchar_t ch) {
    size_t len;
    if (ch < 0x20 || ch == 0x7F) return;
    len = strlen(buf);
    if (len + 4 >= cap) return;
    if (ch < 0x80) {
        buf[len++] = (char)ch;
    } else if (ch < 0x800) {
        buf[len++] = (char)(0xC0 | (ch >> 6));
        buf[len++] = (char)(0x80 | (ch & 0x3F));
    } else {
        buf[len++] = (char)(0xE0 | (ch >> 12));
        buf[len++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        buf[len++] = (char)(0x80 | (ch & 0x3F));
    }
    buf[len] = '\0';
}

static char *reg_field(int i) {
    switch (i) {
    case 0: return s_reg.name;
    case 1: return s_reg.reader_no;
    case 2: return s_reg.password;
    case 3: return s_reg.confirm;
    case 4: return s_reg.phone;
    default: return s_reg.email;
    }
}

static size_t reg_cap(int i) {
    return (i == 1 || i == 4) ? 32 : 64;
}

/* ---------- 鼠标状态 ---------- */
static bool s_left_down = false;

/* 左键"刚按下"沿检测（按下一次只触发一次动作） */
static bool left_press_edge(const InputEvent *ev) {
    if (ev->type != EV_MOUSE) return false;
    if (ev->button & 1) {
        if (!s_left_down) {
            s_left_down = true;
            return true;
        }
    } else {
        s_left_down = false;
    }
    return false;
}

static void update_btn_hover(const InputEvent *ev, Widget **btns, int n) {
    int i;
    for (i = 0; i < n; i++) widget_set_hover(btns[i], widget_hit(btns[i], ev->x, ev->y));
}

/* ---------- 控件构建（一次性） ---------- */
static void ui_build_static(void) {
    if (g_welcome_btns[0]) return;
    g_welcome_btns[0] = widget_button(41, 10, 18, "1. 登录");
    g_welcome_btns[1] = widget_button(41, 12, 18, "2. 注册");
    g_welcome_btns[2] = widget_button(41, 14, 18, "3. 帮助");
    g_welcome_btns[3] = widget_button(41, 16, 18, "4. 退出");
    g_login_btns[0] = widget_button(36, 9, 12, "登录");
    g_login_btns[1] = widget_button(52, 9, 12, "返回");
    g_reg_btns[0] = widget_button(36, 13, 12, "注册");
    g_reg_btns[1] = widget_button(52, 13, 12, "返回");
    g_admin_list = widget_list(5, 4, 30, 12);
    g_reader_list = widget_list(5, 4, 30, 8);
    g_help_btn = widget_button(44, 24, 12, "返回");
}

/* ---------- 页面进入回调 ---------- */
static void on_enter(ScreenId id) {
    switch (id) {
    case SCREEN_LOGIN:
        memset(&s_login, 0, sizeof s_login);
        break;
    case SCREEN_REGISTER:
        memset(&s_reg, 0, sizeof s_reg);
        break;
    case SCREEN_ADMIN_MENU:
        widget_list_set_items(g_admin_list, ADMIN_ITEMS, 8);
        widget_list_set_sel(g_admin_list, 0);
        break;
    case SCREEN_READER_CENTER:
        widget_list_set_items(g_reader_list, READER_ITEMS, 5);
        widget_list_set_sel(g_reader_list, 0);
        break;
    default:
        break;
    }
}

/* ---------- 路由 ---------- */
Result screen_goto(ScreenId id) {
    if (id < 0 || id >= SCREEN_COUNT) return RES_ERR_INVALID;
    if (s_current != id && s_history_len < 32) s_history[s_history_len++] = s_current;
    s_current = id;
    on_enter(id);
    return RES_OK;
}

static void jump_to(ScreenId id) { /* 直接切换，不压历史栈 */
    s_current = id;
    on_enter(id);
}

void screen_back(void) {
    if (s_history_len > 0) {
        s_current = s_history[--s_history_len];
        on_enter(s_current);
    }
}

void screen_message(const char *text) {
    if (!text) {
        s_msg[0] = '\0';
        return;
    }
    str_copy(s_msg, sizeof s_msg, text);
}

/* ---------- 公共绘制 ---------- */
static void draw_title_bar(void) {
    char buf[256];
    const char *pg = (s_current >= 0 && s_current < SCREEN_COUNT) ? SCREEN_NAMES[s_current] : "?";
    int w = s_cols > 250 ? 250 : s_cols;
    int len;
    console_set_color((unsigned short)(g_theme.title_bg | g_theme.title_fg));
    snprintf(buf, sizeof buf, "  宇宙图书馆管理系统 v0.1   [%s]", pg);
    len = (int)strlen(buf);
    if (len > w) len = w;
    if (w > len) memset(buf + len, ' ', (size_t)(w - len));
    buf[w] = '\0';
    console_write_at(0, 0, buf);
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
}

static void draw_status_bar(void) {
    char buf[256];
    char now[24];
    const char *role = "访客", *user = "未登录";
    int w = s_cols > 250 ? 250 : s_cols;
    int len;
    if (g_session.logged_in) {
        user = g_session.display_name[0] ? g_session.display_name : g_session.username;
        role = g_session.role == ROLE_ADMIN ? "管理员" : "读者";
    }
    date_now_str(now, sizeof now);
    console_set_color((unsigned short)(g_theme.status_bg | g_theme.status_fg));
    snprintf(buf, sizeof buf,
             "  当前用户: %s | 角色: %s | %s | F1帮助 F5刷新 Esc返回 Ctrl+Q退出",
             user, role, now);
    len = (int)strlen(buf);
    if (len > w) len = w;
    if (w > len) memset(buf + len, ' ', (size_t)(w - len));
    buf[w] = '\0';
    console_write_at(0, s_rows - 1, buf);
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
}

static void draw_msg(int x, int y) {
    if (!s_msg[0]) return;
    console_set_color((unsigned short)(g_theme.bg | g_theme.warn_fg));
    console_write_at(x, y, s_msg);
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
}

/* ---------- 各页面绘制 ---------- */
static void draw_welcome(void) {
    int cx = s_cols / 2;
    int i;
    console_write_at(cx - 16, 4, "┌──────────────────────────────┐");
    console_write_at(cx - 16, 5, "│   宇宙图书馆管理系统 v0.1    │");
    console_write_at(cx - 16, 6, "└──────────────────────────────┘");
    console_write_at(cx - 15, 8, "欢迎使用，请选择操作：");
    for (i = 0; i < 4; i++) widget_draw(g_welcome_btns[i]);
    if (s_cols < 100 || s_rows < 30)
        console_write_at(cx - 15, 18, "窗口过小，建议调整到至少 100×30（F5 刷新）");
    draw_msg(cx - 15, 19);
}

static void draw_login(void) {
    char buf[128];
    int i;
    console_draw_box(30, 3, 40, 10);
    console_write_at(46, 4, "用户登录");
    console_write_at(33, 6, s_login.focus == 0 ? "▶ 账号:" : "  账号:");
    snprintf(buf, sizeof buf, "%-30s", s_login.account);
    console_write_at(39, 6, buf);
    console_write_at(33, 7, s_login.focus == 1 ? "▶ 密码:" : "  密码:");
    for (i = 0; i < (int)strlen(s_login.password) && i < 30; i++) buf[i] = '*';
    for (; i < 30; i++) buf[i] = ' ';
    buf[30] = '\0';
    console_write_at(39, 7, buf);
    for (i = 0; i < 2; i++) widget_draw(g_login_btns[i]);
    draw_msg(32, 11);
}

static void draw_register(void) {
    static const char *labels[] = {"姓名", "学号/工号", "密码", "确认密码", "手机号", "邮箱"};
    char buf[128];
    int i;
    console_draw_box(25, 3, 50, 14);
    console_write_at(48, 4, "注册读者");
    for (i = 0; i < 6; i++) {
        const char *val = reg_field(i);
        bool masked = (i == 2 || i == 3);
        size_t n = strlen(val);
        int row = 6 + i;
        int j;
        console_write_at(28, row, i == s_reg.focus ? "▶" : " ");
        snprintf(buf, sizeof buf, "%s:", labels[i]);
        console_write_at(30, row, buf);
        for (j = 0; j < 31; j++) {
            if (j < (int)n) buf[j] = masked ? '*' : val[j];
            else buf[j] = ' ';
        }
        buf[31] = '\0';
        console_write_at(42, row, buf);
    }
    for (i = 0; i < 2; i++) widget_draw(g_reg_btns[i]);
    draw_msg(26, 15);
}

static void draw_admin_menu(void) {
    console_write_at(5, 2, "管理员主菜单");
    widget_draw(g_admin_list);
    console_write_at(5, 17, "提示: ↑↓选择 + Enter 确认；鼠标点击/滚轮可用");
    draw_msg(5, 19);
}

static void draw_reader_center(void) {
    console_write_at(5, 2, "读者个人中心");
    widget_draw(g_reader_list);
    console_write_at(5, 13, "提示: ↑↓选择 + Enter 确认；鼠标点击/滚轮可用");
    draw_msg(5, 15);
}

static void draw_help(void) {
    static const char *lines[] = {
        "操作方式",
        "  鼠标: 单击菜单/按钮/列表行；滚轮滚动列表；双击进详情",
        "  键盘: ↑↓/PageUp/PageDown/Home/End 选择，Enter 确认",
        "        Tab 切换表单字段，Backspace 删除，Esc 返回",
        "全局快捷键",
        "  F1 帮助   F5 刷新   Esc 返回上级   Ctrl+Q 退出",
        "当前进度",
        "  M1: 登录/注册可用；业务功能（图书/借阅等）在 M2/M3 实现",
    };
    int i;
    console_write_at(5, 2, "帮助");
    for (i = 0; i < 8; i++) console_write_at(6, 4 + i, lines[i]);
    widget_draw(g_help_btn);
}

void screen_draw(void) {
    ui_build_static();
    console_get_size(&s_cols, &s_rows);
    if (s_cols < 20) s_cols = 20;
    if (s_rows < 10) s_rows = 10;
    console_clear();
    draw_title_bar();
    switch (s_current) {
    case SCREEN_WELCOME: draw_welcome(); break;
    case SCREEN_LOGIN: draw_login(); break;
    case SCREEN_REGISTER: draw_register(); break;
    case SCREEN_ADMIN_MENU: draw_admin_menu(); break;
    case SCREEN_READER_CENTER: draw_reader_center(); break;
    case SCREEN_HELP: draw_help(); break;
    default: break;
    }
    draw_status_bar();
}

/* ---------- 动作 ---------- */
static void welcome_action(int idx) {
    s_msg[0] = '\0';
    switch (idx) {
    case 0: screen_goto(SCREEN_LOGIN); break;
    case 1: screen_goto(SCREEN_REGISTER); break;
    case 2: screen_goto(SCREEN_HELP); break;
    case 3: g_app_quit = true; break;
    }
}

static void do_login(void) {
    Result r = auth_login(s_login.account, s_login.password);
    if (r == RES_OK) {
        s_history_len = 0;
        s_msg[0] = '\0';
        if (g_session.role == ROLE_ADMIN) jump_to(SCREEN_ADMIN_MENU);
        else jump_to(SCREEN_READER_CENTER);
    } else {
        const char *why = "登录失败";
        if (r == RES_ERR_NOT_FOUND) why = "账号不存在";
        else if (r == RES_ERR_FROZEN) why = "账号已冻结，请联系管理员";
        else if (r == RES_ERR_INVALID) why = "账号或密码错误";
        snprintf(s_msg, sizeof s_msg, "登录失败：%s", why);
        s_login.password[0] = '\0'; /* 保留账号，清空密码（FR-103） */
    }
}

static void do_register(void) {
    Reader r;
    Result rc;
    if (strcmp(s_reg.password, s_reg.confirm) != 0) {
        str_copy(s_msg, sizeof s_msg, "两次输入的密码不一致");
        return;
    }
    memset(&r, 0, sizeof r);
    str_copy(r.name, sizeof r.name, s_reg.name);
    str_copy(r.reader_no, sizeof r.reader_no, s_reg.reader_no);
    str_copy(r.phone, sizeof r.phone, s_reg.phone);
    str_copy(r.email, sizeof r.email, s_reg.email);
    rc = auth_register_reader(&r, s_reg.password);
    if (rc == RES_OK) {
        s_history_len = 0;
        memset(&s_reg, 0, sizeof s_reg);
        str_copy(s_msg, sizeof s_msg, "注册成功，请登录");
        jump_to(SCREEN_WELCOME);
    } else {
        if (rc == RES_ERR_DUP) {
            str_copy(s_msg, sizeof s_msg, "注册失败：该学号/工号已注册");
        } else if (rc == RES_ERR_INVALID) {
            str_copy(s_msg, sizeof s_msg,
                     "注册失败：请检查输入（姓名/学号必填，密码≥6位，联系方式格式正确）");
        } else {
            snprintf(s_msg, sizeof s_msg, "注册失败 (code=%d)", rc);
        }
    }
}

static void admin_action(int idx) {
    static const char *names[] = {"图书管理", "字典管理", "读者管理",
                                  "借阅管理", "预约管理", "罚款管理", "统计报表"};
    if (idx == 7) {
        auth_logout();
        s_history_len = 0;
        s_msg[0] = '\0';
        jump_to(SCREEN_WELCOME);
        return;
    }
    if (idx >= 0 && idx <= 6) {
        snprintf(s_msg, sizeof s_msg, "【%s】将在 M2/M3 里程碑实现，敬请期待", names[idx]);
    }
}

static void reader_action(int idx) {
    static const char *names[] = {"图书检索与详情", "我的借阅", "我的预约", "我的罚款"};
    if (idx == 4) {
        auth_logout();
        s_history_len = 0;
        s_msg[0] = '\0';
        jump_to(SCREEN_WELCOME);
        return;
    }
    if (idx >= 0 && idx <= 3) {
        snprintf(s_msg, sizeof s_msg, "【%s】将在 M2/M3 里程碑实现，敬请期待", names[idx]);
    }
}

/* ---------- 菜单通用处理 ---------- */
typedef void (*MenuAction)(int idx);

static bool handle_menu(const InputEvent *ev, Widget *list, int count, MenuAction act) {
    int sel;
    if (ev->type == EV_KEY && ev->key_down) {
        sel = widget_list_sel(list);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(list, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(list, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(list, 0); return true;
        case VK_END: widget_list_set_sel(list, count - 1); return true;
        case VK_PRIOR: widget_list_set_sel(list, sel - 8); return true;
        case VK_NEXT: widget_list_set_sel(list, sel + 8); return true;
        case VK_RETURN: act(widget_list_sel(list)); return true;
        default:
            if (ev->ch >= '1' && ev->ch <= '9') {
                int idx = ev->ch - '1';
                if (idx < count) act(idx);
                return true;
            }
            return false;
        }
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(list, widget_list_sel(list) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        widget_get_rect(list, &lx, &ly, &lw, &lh);
        if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
            int row = ev->y - ly;
            if (row < count) {
                widget_list_set_sel(list, row);
                if (left_press_edge(ev)) act(row);
            }
            return true;
        }
        return true; /* 鼠标在别处：仅刷新 */
    }
    return false;
}

/* ---------- 各页面事件处理 ---------- */
static bool handle_welcome(const InputEvent *ev) {
    if (ev->type == EV_KEY && ev->key_down) {
        if (ev->ch >= '1' && ev->ch <= '4') {
            welcome_action(ev->ch - '1');
            return true;
        }
        return false;
    }
    if (ev->type == EV_MOUSE) {
        int i;
        update_btn_hover(ev, g_welcome_btns, 4);
        if (left_press_edge(ev)) {
            for (i = 0; i < 4; i++) {
                if (widget_hit(g_welcome_btns[i], ev->x, ev->y)) {
                    welcome_action(i);
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

static bool handle_login(const InputEvent *ev) {
    if (ev->type == EV_KEY && ev->key_down) {
        if (ev->vk == VK_TAB) { s_login.focus = !s_login.focus; return true; }
        if (ev->vk == VK_RETURN) { do_login(); return true; }
        if (ev->vk == VK_BACK) {
            utf8_backspace(s_login.focus == 0 ? s_login.account : s_login.password);
            return true;
        }
        if (ev->ch >= 0x20) {
            field_append(s_login.focus == 0 ? s_login.account : s_login.password, 64, ev->ch);
            return true;
        }
        return false;
    }
    if (ev->type == EV_MOUSE) {
        update_btn_hover(ev, g_login_btns, 2);
        if (left_press_edge(ev)) {
            if (ev->y == 6 && ev->x >= 33 && ev->x <= 69) s_login.focus = 0;
            else if (ev->y == 7 && ev->x >= 33 && ev->x <= 69) s_login.focus = 1;
            else if (widget_hit(g_login_btns[0], ev->x, ev->y)) do_login();
            else if (widget_hit(g_login_btns[1], ev->x, ev->y)) screen_back();
        }
        return true;
    }
    return false;
}

static bool handle_register(const InputEvent *ev) {
    if (ev->type == EV_KEY && ev->key_down) {
        if (ev->vk == VK_TAB) { s_reg.focus = (s_reg.focus + 1) % 6; return true; }
        if (ev->vk == VK_RETURN) { do_register(); return true; }
        if (ev->vk == VK_BACK) {
            utf8_backspace(reg_field(s_reg.focus));
            return true;
        }
        if (ev->ch >= 0x20) {
            field_append(reg_field(s_reg.focus), reg_cap(s_reg.focus), ev->ch);
            return true;
        }
        return false;
    }
    if (ev->type == EV_MOUSE) {
        int i;
        update_btn_hover(ev, g_reg_btns, 2);
        if (left_press_edge(ev)) {
            for (i = 0; i < 6; i++) {
                if (ev->y == 6 + i && ev->x >= 28 && ev->x <= 73) {
                    s_reg.focus = i;
                    return true;
                }
            }
            if (widget_hit(g_reg_btns[0], ev->x, ev->y)) do_register();
            else if (widget_hit(g_reg_btns[1], ev->x, ev->y)) screen_back();
        }
        return true;
    }
    return false;
}

static bool handle_help(const InputEvent *ev) {
    if (ev->type == EV_MOUSE) {
        widget_set_hover(g_help_btn, widget_hit(g_help_btn, ev->x, ev->y));
        if (left_press_edge(ev) && widget_hit(g_help_btn, ev->x, ev->y)) {
            screen_back();
            return true;
        }
        return true;
    }
    return false;
}

/* ---------- 主分发 ---------- */
bool screen_handle(const InputEvent *ev) {
    if (!ev) return false;

    if (ev->type == EV_RESIZE) return true; /* 重绘 */

    if (ev->type == EV_KEY && ev->key_down) {
        if (ev->vk == VK_F1) { screen_goto(SCREEN_HELP); return true; }
        if (ev->vk == VK_F5) return true; /* 重绘 */
        if (ev->vk == 'Q' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            g_app_quit = true;
            return true;
        }
        if (ev->vk == VK_ESCAPE) {
            if (s_current != SCREEN_WELCOME) {
                s_msg[0] = '\0';
                screen_back();
            }
            return true;
        }
    }

    switch (s_current) {
    case SCREEN_WELCOME: return handle_welcome(ev);
    case SCREEN_LOGIN: return handle_login(ev);
    case SCREEN_REGISTER: return handle_register(ev);
    case SCREEN_ADMIN_MENU: return handle_menu(ev, g_admin_list, 8, admin_action);
    case SCREEN_READER_CENTER: return handle_menu(ev, g_reader_list, 5, reader_action);
    case SCREEN_HELP: return handle_help(ev);
    default: return false;
    }
}

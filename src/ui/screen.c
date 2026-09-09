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
#include "../core/book.h"
#include "../core/fine.h"
#include "../core/loan.h"
#include "../core/reader.h"
#include "../core/reservation.h"
#include "../core/search.h"
#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/hash.h"
#include "../utils/string.h"
#include "../utils/validator.h"

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

/* ================= M2 状态：图书检索 / 图书表单 / 字典 / 修改密码 ================ */

typedef enum { BF_ADD, BF_EDIT, BF_VIEW } BfMode;

typedef struct {
    BfMode mode;
    EntityId id; /* BF_EDIT/BF_VIEW 时的图书 id */
    char isbn[32], title[256], author[64], publisher[64], category[64];
    char pub_date[16], price[16], copies[16], location[64], desc[120];
    int focus; /* 0..9 */
} BfState;

typedef enum { DICT_AUTHOR, DICT_PUB, DICT_CAT } DictKind;

typedef struct {
    int mode;       /* 0=列表 1=表单 */
    DictKind kind;  /* 当前字典类型 */
    EntityId edit_id; /* 表单模式：0=新增，否则编辑该 id */
    char name[128], opt1[256], opt2[64], desc[512];
    int focus;
} DictState;

typedef struct {
    char old_pwd[64], new_pwd[64], confirm[64];
    int focus; /* 0..2 */
} PwdForm;

static BfState s_bf;
static DictState s_dict;
static PwdForm s_pwd;
static char s_bs_keyword[64];      /* 图书检索关键字 */
static int s_bs_field = 0;         /* 0ISBN 1书名 2作者 3分类 */
static bool s_bs_typing = false;   /* 正在输入关键字 */
#define BS_MAX 512
static const Book *s_bs_res[BS_MAX]; /* 检索结果（指向仓库记录） */
static size_t s_bs_count = 0;
static bool s_bs_rebuild = false;  /* 列表需重建 */
static bool s_dict_rebuild = false;
static EntityId *s_dict_ids = NULL; /* 字典列表行 → id 映射 */
static size_t s_dict_ids_count = 0;
static EntityId s_pending_del = 0; /* 待二次确认删除的 id */
static int s_pending_kind = 0;     /* 0图书 1作者 2出版社 3分类 */

/* ================= M3 状态：读者 / 借阅 / 预约 / 罚款 ================= */
#define M3_MAX 256

/* 读者管理 */
static int s_rl_mode;        /* 0=列表 1=表单 */
static char s_rl_keyword[64];
static bool s_rl_typing;
static const Reader *s_rl_res[M3_MAX];
static size_t s_rl_count;
static bool s_rl_rebuild;
static EntityId s_rl_edit_id; /* 0=新增 */
static char s_rl_name[64], s_rl_no[32], s_rl_phone[32], s_rl_email[64], s_rl_pwd[64];
static int s_rl_focus;

/* 借阅管理 */
static int s_loan_tab;   /* 0=借出(仅管理员) 1=在借 2=逾期 */
static char s_loan_no[32], s_loan_isbn[32];
static int s_loan_focus;
static const Loan *s_loan_res[M3_MAX];
static size_t s_loan_count;
static bool s_loan_rebuild;
static bool s_loan_mine; /* 读者端：只看自己的 */

/* 预约管理 */
static const Reservation *s_res_res[M3_MAX];
static size_t s_res_count;
static bool s_res_rebuild;
static bool s_res_mine;

/* 罚款管理 */
static int s_fine_tab; /* 0=未缴 1=已缴 */
static const Fine *s_fine_res[M3_MAX];
static size_t s_fine_count;
static bool s_fine_rebuild;
static bool s_fine_mine;

/* ---------- 静态控件 ---------- */
static Widget *g_welcome_list;
static Widget *g_login_btns[2];
static Widget *g_reg_btns[2];
static Widget *g_admin_list;
static Widget *g_reader_list;
static Widget *g_help_btn;
/* M2: 图书管理 */
static Widget *g_book_list;
static Widget *g_ba_new, *g_ba_view, *g_ba_edit, *g_ba_del, *g_ba_sus, *g_ba_back;
static Widget *g_bf_save, *g_bf_cancel, *g_bf_back;
/* M2: 字典管理 */
static Widget *g_dk_auth, *g_dk_pub, *g_dk_cat;
static Widget *g_dict_list;
static Widget *g_da_new, *g_da_edit, *g_da_del, *g_da_back;
static Widget *g_df_save, *g_df_cancel;
/* M2: 修改密码 */
static Widget *g_pwd_ok, *g_pwd_cancel;
/* M3: 读者管理 */
static Widget *g_reader_lw;
static Widget *g_rl_new, *g_rl_edit, *g_rl_freeze, *g_rl_del, *g_rl_view, *g_rl_back;
static Widget *g_rl_save, *g_rl_cancel;
/* M3: 借阅管理 */
static Widget *g_loan_lw;
static Widget *g_ln_tab0, *g_ln_tab1, *g_ln_tab2;
static Widget *g_ln_borrow, *g_ln_back;
/* M3: 预约管理 */
static Widget *g_res_lw;
static Widget *g_rs_cancel, *g_rs_proc, *g_rs_back;
/* M3: 罚款管理 */
static Widget *g_fine_lw;
static Widget *g_fi_tab0, *g_fi_tab1;
static Widget *g_fi_pay, *g_fi_back;

static const char *WELCOME_ITEMS[] = {
    "   登  录", "   注  册", "   帮  助", "   退  出"
};

static const char *ADMIN_ITEMS[] = {
    "1. 图书管理", "2. 字典管理", "3. 读者管理",
    "4. 借阅管理", "5. 预约管理", "6. 罚款管理",
    "7. 统计报表", "8. 修改密码", "9. 退出登录"
};
static const char *READER_ITEMS[] = {
    "1. 图书检索与详情", "2. 我的借阅", "3. 我的预约",
    "4. 我的罚款", "5. 修改密码", "6. 退出登录"
};

static const char *SCREEN_NAMES[SCREEN_COUNT] = {
    "欢迎", "登录", "注册", "管理员主菜单", "图书列表", "图书表单",
    "读者管理", "借阅管理", "预约管理", "罚款管理", "统计报表",
    "读者个人中心", "帮助", "字典管理", "修改密码"
};

/* ---------- 前置声明 ---------- */
static void welcome_action(int idx);
static void do_login(void);
static void do_register(void);
static void admin_action(int idx);
static void reader_action(int idx);
static void jump_to(ScreenId id);
/* M2 页面（定义见文件尾部） */
static void bs_run_search(void);
static void draw_book_list(void);
static void draw_book_form(void);
static void draw_dict(void);
static void draw_change_pwd(void);
static bool handle_book_list(const InputEvent *ev);
static bool handle_book_form(const InputEvent *ev);
static bool handle_dict(const InputEvent *ev);
static bool handle_change_pwd(const InputEvent *ev);
/* M3 页面（定义见文件尾部） */
static void draw_reader_list(void);
static void draw_loan_manage(void);
static void draw_reservation_manage(void);
static void draw_fine_manage(void);
static bool handle_reader_list(const InputEvent *ev);
static bool handle_loan_manage(const InputEvent *ev);
static bool handle_reservation_manage(const InputEvent *ev);
static bool handle_fine_manage(const InputEvent *ev);
static void rl_run_search(void);
static void loan_refresh(void);
static void res_refresh(void);
static void fine_refresh(void);

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

/* 更新按钮悬停状态；返回"是否有变化"（用于避免无意义的整屏重绘） */
static bool hover_changed(const InputEvent *ev, Widget **btns, int n) {
    bool changed = false;
    int i;
    for (i = 0; i < n; i++) {
        bool h = widget_hit(btns[i], ev->x, ev->y);
        if (h != widget_get_hover(btns[i])) {
            widget_set_hover(btns[i], h);
            changed = true;
        }
    }
    return changed;
}

/* ---------- 控件构建（一次性） ---------- */
static void ui_build_static(void) {
    if (g_login_btns[0]) return;
    g_welcome_list = widget_list(0, 0, 26, 4);
    g_login_btns[0] = widget_button(0, 0, 14, "登 录");
    g_login_btns[1] = widget_button(52, 9, 12, "返回");
    g_reg_btns[0] = widget_button(36, 13, 12, "注册");
    g_reg_btns[1] = widget_button(52, 13, 12, "返回");
    g_admin_list = widget_list(5, 4, 30, 12);
    g_reader_list = widget_list(5, 4, 30, 8);
    g_help_btn = widget_button(44, 24, 12, "返回");
    /* M2: 图书列表页 */
    g_book_list = widget_list(2, 5, 76, 12);
    g_ba_new = widget_button(2, 20, 10, "A 新增");
    g_ba_view = widget_button(14, 20, 10, "V 详情");
    g_ba_edit = widget_button(26, 20, 10, "E 编辑");
    g_ba_del = widget_button(38, 20, 10, "D 删除");
    g_ba_sus = widget_button(50, 20, 10, "S 停借/恢复");
    g_ba_back = widget_button(64, 20, 10, "Esc 返回");
    g_bf_save = widget_button(40, 17, 12, "保存");
    g_bf_cancel = widget_button(56, 17, 12, "取消");
    g_bf_back = widget_button(48, 17, 12, "返回");
    /* M2: 字典管理页 */
    g_dk_auth = widget_button(2, 2, 10, "1 作者");
    g_dk_pub = widget_button(14, 2, 10, "2 出版社");
    g_dk_cat = widget_button(26, 2, 10, "3 分类");
    g_dict_list = widget_list(2, 4, 50, 12);
    g_da_new = widget_button(2, 18, 10, "A 新增");
    g_da_edit = widget_button(14, 18, 10, "E 编辑");
    g_da_del = widget_button(26, 18, 10, "D 删除");
    g_da_back = widget_button(38, 18, 10, "Esc 返回");
    g_df_save = widget_button(40, 10, 12, "保存");
    g_df_cancel = widget_button(56, 10, 12, "取消");
    /* M2: 修改密码页 */
    g_pwd_ok = widget_button(36, 10, 12, "确认");
    g_pwd_cancel = widget_button(52, 10, 12, "取消");
    /* M3: 读者管理 */
    g_reader_lw = widget_list(4, 6, 90, 14);
    g_rl_new = widget_button(4, 0, 12, "A 新增");
    g_rl_edit = widget_button(18, 0, 12, "E 编辑");
    g_rl_freeze = widget_button(32, 0, 16, "F 冻结/解冻");
    g_rl_del = widget_button(50, 0, 12, "D 注销");
    g_rl_view = widget_button(64, 0, 12, "V 详情");
    g_rl_back = widget_button(78, 0, 12, "Esc 返回");
    g_rl_save = widget_button(0, 0, 14, "保存");
    g_rl_cancel = widget_button(0, 0, 14, "取消");
    /* M3: 借阅管理 */
    g_loan_lw = widget_list(4, 6, 90, 14);
    g_ln_tab0 = widget_button(4, 1, 14, "1 借出");
    g_ln_tab1 = widget_button(20, 1, 14, "2 在借");
    g_ln_tab2 = widget_button(36, 1, 14, "3 逾期");
    g_ln_borrow = widget_button(0, 0, 14, "借出");
    g_ln_back = widget_button(78, 0, 12, "Esc 返回");
    /* M3: 预约管理 */
    g_res_lw = widget_list(4, 6, 90, 14);
    g_rs_cancel = widget_button(4, 0, 12, "C 取消预约");
    g_rs_proc = widget_button(20, 0, 16, "P 处理下一位");
    g_rs_back = widget_button(78, 0, 12, "Esc 返回");
    /* M3: 罚款管理 */
    g_fine_lw = widget_list(4, 6, 90, 14);
    g_fi_tab0 = widget_button(4, 1, 14, "1 未缴");
    g_fi_tab1 = widget_button(20, 1, 14, "2 已缴");
    g_fi_pay = widget_button(0, 0, 14, "收款");
    g_fi_back = widget_button(78, 0, 12, "Esc 返回");
}

/* ---------- 页面进入回调 ---------- */
static void on_enter(ScreenId id) {
    switch (id) {
    case SCREEN_WELCOME:
        widget_list_set_items(g_welcome_list, WELCOME_ITEMS, 4);
        widget_list_set_sel(g_welcome_list, 0);
        break;
    case SCREEN_LOGIN:
        memset(&s_login, 0, sizeof s_login);
        break;
    case SCREEN_REGISTER:
        memset(&s_reg, 0, sizeof s_reg);
        break;
    case SCREEN_ADMIN_MENU:
        widget_list_set_items(g_admin_list, ADMIN_ITEMS, 9);
        widget_list_set_sel(g_admin_list, 0);
        break;
    case SCREEN_READER_CENTER:
        widget_list_set_items(g_reader_list, READER_ITEMS, 6);
        widget_list_set_sel(g_reader_list, 0);
        break;
    case SCREEN_BOOK_LIST:
        bs_run_search(); /* 保留关键字，按当前字段刷新（首次为空 → 全部） */
        break;
    case SCREEN_DICT_MANAGE:
        s_dict.mode = 0;
        s_dict_rebuild = true;
        break;
    case SCREEN_CHANGE_PWD:
        memset(&s_pwd, 0, sizeof s_pwd);
        break;
    case SCREEN_READER_LIST:
        if (s_rl_mode != 1) {
            s_rl_mode = 0;
            rl_run_search();
        }
        break;
    case SCREEN_LOAN_MANAGE:
        loan_refresh();
        break;
    case SCREEN_RESERVATION_MANAGE:
        res_refresh();
        break;
    case SCREEN_FINE_MANAGE:
        fine_refresh();
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

/* =====================================================================
 * 布局与绘制工具
 * ===================================================================== */

/* 颜色快捷宏（属性 = 背景<<4 | 前景） */
#define COL_BODY   ((unsigned short)(g_theme.bg | g_theme.fg))
#define COL_BORDER ((unsigned short)(g_theme.bg | g_theme.border_fg))
#define COL_TITLE  ((unsigned short)(g_theme.bg | g_theme.title_fg))
#define COL_ACCENT ((unsigned short)(g_theme.bg | g_theme.accent_fg))
#define COL_HINT   ((unsigned short)(g_theme.bg | g_theme.hint_fg))
#define COL_OK     ((unsigned short)(g_theme.bg | g_theme.ok_fg))
#define COL_WARN   ((unsigned short)(g_theme.bg | g_theme.warn_fg))
#define COL_ERR    ((unsigned short)(g_theme.bg | g_theme.error_fg))

/* 当前帧布局（draw 时计算，handle 复用，保证命中检测与显示一致） */
static int L_px, L_py, L_pw, L_ph; /* 主面板 */
static int L_fx, L_fy, L_fw;       /* 表单：值区 x / 首行 y / 宽 */
static int L_msg_y;                /* 消息行 */
static int L_sx;                   /* 检索条起始 x */

/* CJK 等宽字符判定 */
static bool ui_is_wide(unsigned int cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||
           (cp >= 0x2E80 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6);
}

/* UTF-8 字符串的显示宽度（列数） */
static int ui_width(const char *s) {
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    if (!s) return 0;
    while (*p) {
        if (p[0] < 0x80) { w += 1; p += 1; }
        else if ((p[0] & 0xE0) == 0xC0) { w += 1; p += 2; }
        else if ((p[0] & 0xF0) == 0xE0) {
            unsigned int cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            w += ui_is_wide(cp) ? 2 : 1;
            p += 3;
        } else { w += 1; p += 4; }
    }
    return w;
}

/* 按显示宽度截断并补空格到 width 列（用于表格列对齐） */
static void ui_pad(char *dst, size_t cap, const char *src, int width) {
    size_t n = 0;
    int w = 0;
    const unsigned char *p = (const unsigned char *)src;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    while (*p && w < width) {
        size_t len = 1;
        int cw = 1;
        if (p[0] < 0x80) { len = 1; cw = 1; }
        else if ((p[0] & 0xE0) == 0xC0) { len = 2; cw = 1; }
        else if ((p[0] & 0xF0) == 0xE0) {
            unsigned int cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            len = 3;
            cw = ui_is_wide(cp) ? 2 : 1;
        } else { len = 4; cw = 1; }
        if (w + cw > width) break;
        if (n + len + 1 >= cap) break;
        memcpy(dst + n, p, len);
        n += len;
        w += cw;
        p += len;
    }
    while (w < width && n + 1 < cap) {
        dst[n++] = ' ';
        w++;
    }
    dst[n] = '\0';
}

/* 追加一列（按显示宽度对齐）到 out */
static void ui_append(char *out, size_t cap, const char *text, int width) {
    char tmp[160];
    size_t used = strlen(out);
    size_t n;
    ui_pad(tmp, sizeof tmp, text, width);
    n = strlen(tmp);
    if (used + n + 1 >= cap) return;
    memcpy(out + used, tmp, n + 1);
}

/* 居中输出一行 */
static void ui_center(int y, const char *text) {
    int x = (s_cols - ui_width(text)) / 2;
    if (x < 0) x = 0;
    console_write_at(x, y, text);
}

/* 用空格填满一行并设置底色 */
static void ui_fill_row(int y, unsigned short attr) {
    char line[256];
    int n = s_cols > 250 ? 250 : s_cols;
    if (n < 1) return;
    memset(line, ' ', (size_t)n);
    line[n] = '\0';
    console_set_color(attr);
    console_write_at(0, y, line);
}

/* 带标题的面板 */
static void ui_panel(int x, int y, int w, int h, const char *title) {
    char buf[128];
    if (w < 4 || h < 3) return;
    console_set_color(COL_BORDER);
    console_draw_box(x, y, w, h);
    if (title && *title) {
        snprintf(buf, sizeof buf, " %s ", title);
        console_set_color(COL_TITLE);
        console_write_at(x + (w - ui_width(buf)) / 2, y, buf);
    }
    console_set_color(COL_BODY);
}

/* 面板内水平分隔线 */
static void ui_sep(int x, int y, int w) {
    char line[256 * 3 + 1];
    int i, n = w > 250 ? 250 : w;
    if (n < 1) return;
    line[0] = '\0';
    for (i = 0; i < n; i++) strcat(line, "─");
    console_set_color(COL_BORDER);
    console_write_at(x, y, line);
    console_set_color(COL_BODY);
}

/* 底部提示行（状态栏上方） */
static void draw_hint_bar(const char *text) {
    if (!text) return;
    console_set_color(COL_HINT);
    ui_center(s_rows - 2, text);
    console_set_color(COL_BODY);
}

/* ---------- 公共绘制 ---------- */
static void draw_title_bar(void) {
    char right[64];
    const char *pg = (s_current >= 0 && s_current < SCREEN_COUNT) ? SCREEN_NAMES[s_current] : "?";
    unsigned short attr = (unsigned short)(g_theme.title_bg | g_theme.title_fg);
    ui_fill_row(0, attr);
    console_set_color(attr);
    console_write_at(2, 0, "宇宙图书馆管理系统");
    console_set_color(COL_HINT);
    console_write_at(22, 0, "v0.1");
    console_set_color(attr);
    snprintf(right, sizeof right, "[ %s ]", pg);
    console_write_at(s_cols - ui_width(right) - 2, 0, right);
    console_set_color(COL_BODY);
}

static void draw_status_bar(void) {
    char left[128], now[24];
    const char *role = "访客", *user = "未登录";
    const char *hints = "F1 帮助   F5 刷新   Esc 返回   Ctrl+Q 退出";
    unsigned short attr = (unsigned short)(g_theme.status_bg | g_theme.status_fg);
    if (g_session.logged_in) {
        user = g_session.display_name[0] ? g_session.display_name : g_session.username;
        role = g_session.role == ROLE_ADMIN ? "管理员" : "读者";
    }
    date_now_str(now, sizeof now);
    ui_fill_row(s_rows - 1, attr);
    console_set_color(attr);
    snprintf(left, sizeof left, " %s · %s · %s", user, role, now);
    console_write_at(1, s_rows - 1, left);
    console_write_at(s_cols - ui_width(hints) - 2, s_rows - 1, hints);
    console_set_color(COL_BODY);
}

static void draw_msg(int x, int y) {
    if (!s_msg[0]) return;
    console_set_color(COL_WARN);
    console_write_at(x, y, s_msg);
    console_set_color(COL_BODY);
}

/* ---------- 各页面绘制 ---------- */
static void draw_welcome(void) {
    int pw = 56, ph = 5, px, py = 2;
    int lw = 26, lx, ly;
    if (pw > s_cols - 8) pw = s_cols - 8;
    px = (s_cols - pw) / 2;

    ui_panel(px, py, pw, ph, NULL);
    console_set_color(COL_TITLE);
    ui_center(py + 1, "宇宙图书馆管理系统");
    console_set_color(COL_HINT);
    ui_center(py + 2, "Library Management System");
    console_set_color(COL_BODY);

    console_set_color(COL_HINT);
    ui_center(py + ph + 1, "请选择操作（↑↓ 选择 · Enter 确认 · 鼠标可点击）");
    console_set_color(COL_BODY);

    lx = (s_cols - lw) / 2;
    ly = py + ph + 3;
    widget_set_rect(g_welcome_list, lx, ly, lw, 4);
    widget_draw(g_welcome_list);

    if (s_cols < 100 || s_rows < 30) {
        console_set_color(COL_WARN);
        ui_center(s_rows - 4, "窗口偏小，建议调整到至少 100×30（F5 刷新）");
        console_set_color(COL_BODY);
    }
    draw_msg(lx, ly + 6);
}

static void draw_login(void) {
    char buf[96];
    int pw = 46, ph = 11, px, py = 4, i;
    if (pw > s_cols - 8) pw = s_cols - 8;
    px = (s_cols - pw) / 2;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;
    L_fx = px + 12;
    L_fw = pw - 16;
    L_fy = py + 3;
    L_msg_y = py + ph - 2;

    ui_panel(px, py, pw, ph, "用户登录");
    console_set_color(COL_HINT);
    console_write_at(px + 3, py + 1, "请输入账号与密码（Tab 切换 · Enter 提交）");
    console_set_color(COL_BODY);

    /* 账号 */
    console_set_color(s_login.focus == 0 ? COL_ACCENT : COL_BODY);
    console_write_at(px + 4, L_fy, "账号");
    console_set_color(COL_BODY);
    ui_pad(buf, sizeof buf, s_login.account, L_fw);
    console_set_color(s_login.focus == 0 ? COL_BODY : COL_HINT);
    console_write_at(L_fx, L_fy, buf);

    /* 密码（掩码） */
    console_set_color(s_login.focus == 1 ? COL_ACCENT : COL_BODY);
    console_write_at(px + 4, L_fy + 1, "密码");
    console_set_color(COL_BODY);
    for (i = 0; i < (int)strlen(s_login.password) && i < L_fw - 1; i++) buf[i] = '*';
    for (; i < L_fw; i++) buf[i] = ' ';
    buf[L_fw] = '\0';
    console_set_color(s_login.focus == 1 ? COL_BODY : COL_HINT);
    console_write_at(L_fx, L_fy + 1, buf);
    console_set_color(COL_BODY);

    ui_sep(px + 1, py + 6, pw - 2);
    widget_set_rect(g_login_btns[0], px + 7, py + 7, 14, 1);
    widget_set_rect(g_login_btns[1], px + pw - 21, py + 7, 14, 1);
    widget_draw(g_login_btns[0]);
    widget_draw(g_login_btns[1]);
    draw_msg(px + 3, L_msg_y);
}

static void draw_register(void) {
    static const char *labels[] = {"姓名", "学号/工号", "密码", "确认密码", "手机号", "邮箱"};
    char buf[128];
    int pw = 54, ph = 15, px, py = 3, i, j;
    if (pw > s_cols - 8) pw = s_cols - 8;
    px = (s_cols - pw) / 2;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;
    L_fx = px + 14;
    L_fw = pw - 18;
    L_fy = py + 3;
    L_msg_y = py + ph - 2;

    ui_panel(px, py, pw, ph, "注册读者");
    console_set_color(COL_HINT);
    console_write_at(px + 3, py + 1, "Tab 切换字段 · Enter 提交 · Esc 返回");
    console_set_color(COL_BODY);

    for (i = 0; i < 6; i++) {
        const char *val = reg_field(i);
        bool masked = (i == 2 || i == 3);
        int row = L_fy + i;
        size_t n = strlen(val);
        console_set_color(i == s_reg.focus ? COL_ACCENT : COL_BODY);
        console_write_at(px + 4, row, labels[i]);
        console_set_color(COL_BODY);
        for (j = 0; j < L_fw; j++) {
            if (j < (int)n) buf[j] = masked ? '*' : val[j];
            else buf[j] = ' ';
        }
        buf[L_fw] = '\0';
        console_write_at(L_fx, row, buf);
    }

    ui_sep(px + 1, py + 10, pw - 2);
    widget_set_rect(g_reg_btns[0], px + 9, py + 11, 14, 1);
    widget_set_rect(g_reg_btns[1], px + pw - 23, py + 11, 14, 1);
    widget_draw(g_reg_btns[0]);
    widget_draw(g_reg_btns[1]);
    draw_msg(px + 3, L_msg_y);
}

static void draw_admin_menu(void) {
    int pw = 34, ph = 13, px, py = 3, sw = 36;
    char line[80];
    size_t nb = 0, nr = 0, i;
    const Book *books;
    int total = 0, avail = 0, susp = 0;

    if (pw + sw + 4 > s_cols) sw = s_cols - pw - 6;
    if (sw < 20) sw = 20;
    px = (s_cols - (pw + 2 + sw)) / 2;
    if (px < 1) px = 1;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;

    ui_panel(px, py, pw, ph, "管理员主菜单");
    widget_set_rect(g_admin_list, px + 2, py + 2, pw - 4, ph - 3);
    widget_draw(g_admin_list);

    /* 右侧：馆藏概览 */
    books = repo_books(&nb);
    repo_readers(&nr);
    for (i = 0; i < nb; i++) {
        if (books[i].status == BOOK_STATUS_DELETED) continue;
        total++;
        if (books[i].status == BOOK_STATUS_SUSPENDED) susp++;
        else avail += books[i].available_copies;
    }
    ui_panel(px + pw + 2, py, sw, ph, "馆藏概览");
    console_set_color(COL_HINT);
    console_write_at(px + pw + 4, py + 2, "实时统计");
    console_set_color(COL_BODY);
    snprintf(line, sizeof line, "图书总数     %d", total);
    console_write_at(px + pw + 4, py + 4, line);
    snprintf(line, sizeof line, "在架可借     %d", avail);
    console_write_at(px + pw + 4, py + 5, line);
    snprintf(line, sizeof line, "停借图书     %d", susp);
    console_write_at(px + pw + 4, py + 6, line);
    snprintf(line, sizeof line, "读者人数     %zu", nr);
    console_write_at(px + pw + 4, py + 7, line);
    console_set_color(COL_HINT);
    console_write_at(px + pw + 4, py + ph - 2, "数据实时来自 CSV");
    console_set_color(COL_BODY);

    draw_hint_bar("↑↓ 选择 · Enter 确认 · 鼠标点击/滚轮可用");
    draw_msg(px, py + ph + 1);
}

static void draw_reader_center(void) {
    int pw = 34, ph = 11, px, py = 3, sw = 34;
    if (pw + sw + 4 > s_cols) sw = s_cols - pw - 6;
    if (sw < 20) sw = 20;
    px = (s_cols - (pw + 2 + sw)) / 2;
    if (px < 1) px = 1;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;

    ui_panel(px, py, pw, ph, "读者个人中心");
    widget_set_rect(g_reader_list, px + 2, py + 2, pw - 4, ph - 3);
    widget_draw(g_reader_list);

    ui_panel(px + pw + 2, py, sw, ph, "我的信息");
    console_set_color(COL_HINT);
    console_write_at(px + pw + 4, py + 2, "当前登录读者");
    console_set_color(COL_BODY);
    console_write_at(px + pw + 4, py + 4, g_session.display_name);
    console_set_color(COL_HINT);
    console_write_at(px + pw + 4, py + 5, g_session.username);
    console_write_at(px + pw + 4, py + ph - 3, "借阅 / 预约 / 罚款");
    console_write_at(px + pw + 4, py + ph - 2, "已开放 · 统计报表在 M4");
    console_set_color(COL_BODY);

    draw_hint_bar("↑↓ 选择 · Enter 确认 · 鼠标点击/滚轮可用");
    draw_msg(px, py + ph + 1);
}

static void draw_help(void) {
    static const char *lines[] = {
        "鼠标操作",
        "  单击 菜单 / 按钮 / 列表行          滚轮 滚动列表",
        "键盘操作",
        "  ↑↓ PageUp/PageDown Home/End 选择        Enter 确认",
        "  Tab 切换表单字段     Backspace 删除     Esc 返回",
        "全局快捷键",
        "  F1 帮助        F5 刷新        Esc 返回上级        Ctrl+Q 退出",
        "功能进度",
        "  M1 登录/注册  ·  M2 图书与字典管理  ·  M3 借阅/预约/罚款  ·  M4 报表",
    };
    int pw = 76, ph = 14, px, py = 3, i;
    if (pw > s_cols - 6) pw = s_cols - 6;
    px = (s_cols - pw) / 2;

    ui_panel(px, py, pw, ph, "帮助");
    for (i = 0; i < 9; i++) {
        bool header = (lines[i][0] != ' ');
        console_set_color(header ? COL_TITLE : COL_BODY);
        console_write_at(px + 3, py + 2 + i, lines[i]);
    }
    console_set_color(COL_BODY);
    widget_set_rect(g_help_btn, (s_cols - 14) / 2, py + ph + 1, 14, 1);
    widget_draw(g_help_btn);
}

void screen_draw(void) {
    ui_build_static();
    console_get_size(&s_cols, &s_rows);
    if (s_cols < 40) s_cols = 40;
    if (s_rows < 16) s_rows = 16;
    console_clear();
    draw_title_bar();
    switch (s_current) {
    case SCREEN_WELCOME: draw_welcome(); break;
    case SCREEN_LOGIN: draw_login(); break;
    case SCREEN_REGISTER: draw_register(); break;
    case SCREEN_ADMIN_MENU: draw_admin_menu(); break;
    case SCREEN_READER_CENTER: draw_reader_center(); break;
    case SCREEN_HELP: draw_help(); break;
    case SCREEN_BOOK_LIST: draw_book_list(); break;
    case SCREEN_BOOK_EDIT: draw_book_form(); break;
    case SCREEN_DICT_MANAGE: draw_dict(); break;
    case SCREEN_CHANGE_PWD: draw_change_pwd(); break;
    case SCREEN_READER_LIST: draw_reader_list(); break;
    case SCREEN_LOAN_MANAGE: draw_loan_manage(); break;
    case SCREEN_RESERVATION_MANAGE: draw_reservation_manage(); break;
    case SCREEN_FINE_MANAGE: draw_fine_manage(); break;
    default: break;
    }
    draw_status_bar();
    console_present(); /* 后台缓冲一次性刷新（消除闪烁） */
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

static void logout_to_welcome(void) {
    auth_logout();
    s_history_len = 0;
    s_msg[0] = '\0';
    jump_to(SCREEN_WELCOME);
}

static void admin_action(int idx) {
    if (idx == 0) { s_msg[0] = '\0'; screen_goto(SCREEN_BOOK_LIST); return; }
    if (idx == 1) { s_msg[0] = '\0'; screen_goto(SCREEN_DICT_MANAGE); return; }
    if (idx == 2) {
        s_rl_mode = 0;
        s_rl_keyword[0] = '\0';
        s_msg[0] = '\0';
        screen_goto(SCREEN_READER_LIST);
        return;
    }
    if (idx == 3) {
        s_loan_mine = false;
        s_loan_tab = 0;
        s_msg[0] = '\0';
        screen_goto(SCREEN_LOAN_MANAGE);
        return;
    }
    if (idx == 4) { s_res_mine = false; s_msg[0] = '\0'; screen_goto(SCREEN_RESERVATION_MANAGE); return; }
    if (idx == 5) { s_fine_mine = false; s_fine_tab = 0; s_msg[0] = '\0'; screen_goto(SCREEN_FINE_MANAGE); return; }
    if (idx == 6) { str_copy(s_msg, sizeof s_msg, "【统计报表】将在 M4 里程碑实现，敬请期待"); return; }
    if (idx == 7) { s_msg[0] = '\0'; screen_goto(SCREEN_CHANGE_PWD); return; }
    if (idx == 8) { logout_to_welcome(); return; }
}

static void reader_action(int idx) {
    if (idx == 0) { s_msg[0] = '\0'; screen_goto(SCREEN_BOOK_LIST); return; }
    if (idx == 1) { s_loan_mine = true; s_loan_tab = 1; s_msg[0] = '\0'; screen_goto(SCREEN_LOAN_MANAGE); return; }
    if (idx == 2) { s_res_mine = true; s_msg[0] = '\0'; screen_goto(SCREEN_RESERVATION_MANAGE); return; }
    if (idx == 3) { s_fine_mine = true; s_fine_tab = 0; s_msg[0] = '\0'; screen_goto(SCREEN_FINE_MANAGE); return; }
    if (idx == 4) { s_msg[0] = '\0'; screen_goto(SCREEN_CHANGE_PWD); return; }
    if (idx == 5) { logout_to_welcome(); return; }
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
        if (left_press_edge(ev) && ev->y >= ly && ev->y < ly + lh &&
            ev->x >= lx && ev->x < lx + lw) {
            int row = ev->y - ly;
            if (row < count) {
                widget_list_set_sel(list, row);
                act(row);
            }
            return true;
        }
        return false; /* 移动/悬停：菜单列表无悬停高亮，无需重绘 */
    }
    return false;
}

/* ---------- 各页面事件处理 ---------- */

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
        bool changed = hover_changed(ev, g_login_btns, 2);
        if (left_press_edge(ev)) {
            if (ev->y >= L_fy && ev->y < L_fy + 2 &&
                ev->x >= L_fx - 2 && ev->x < L_fx + L_fw) {
                s_login.focus = ev->y - L_fy;
                return true;
            }
            if (widget_hit(g_login_btns[0], ev->x, ev->y)) { do_login(); return true; }
            if (widget_hit(g_login_btns[1], ev->x, ev->y)) { screen_back(); return true; }
        }
        return changed;
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
        bool changed = hover_changed(ev, g_reg_btns, 2);
        if (left_press_edge(ev)) {
            if (ev->y >= L_fy && ev->y < L_fy + 6 &&
                ev->x >= L_fx - 2 && ev->x < L_fx + L_fw) {
                s_reg.focus = ev->y - L_fy;
                return true;
            }
            if (widget_hit(g_reg_btns[0], ev->x, ev->y)) { do_register(); return true; }
            if (widget_hit(g_reg_btns[1], ev->x, ev->y)) { screen_back(); return true; }
        }
        return changed;
    }
    return false;
}

static bool handle_help(const InputEvent *ev) {
    if (ev->type == EV_MOUSE) {
        bool changed = hover_changed(ev, (Widget *[]){g_help_btn}, 1);
        if (left_press_edge(ev) && widget_hit(g_help_btn, ev->x, ev->y)) {
            screen_back();
            return true;
        }
        return changed;
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
            if (s_current == SCREEN_BOOK_LIST && s_bs_typing) {
                s_bs_typing = false; /* 先退出关键字输入 */
                return true;
            }
            if (s_current == SCREEN_DICT_MANAGE && s_dict.mode == 1) {
                s_dict.mode = 0; /* 字典表单 → 列表 */
                s_dict_rebuild = true;
                return true;
            }
            if (s_current != SCREEN_WELCOME) {
                s_msg[0] = '\0';
                screen_back();
            }
            return true;
        }
    }

    switch (s_current) {
    case SCREEN_WELCOME: return handle_menu(ev, g_welcome_list, 4, welcome_action);
    case SCREEN_LOGIN: return handle_login(ev);
    case SCREEN_REGISTER: return handle_register(ev);
    case SCREEN_ADMIN_MENU: return handle_menu(ev, g_admin_list, 9, admin_action);
    case SCREEN_READER_CENTER: return handle_menu(ev, g_reader_list, 6, reader_action);
    case SCREEN_HELP: return handle_help(ev);
    case SCREEN_BOOK_LIST: return handle_book_list(ev);
    case SCREEN_BOOK_EDIT: return handle_book_form(ev);
    case SCREEN_DICT_MANAGE: return handle_dict(ev);
    case SCREEN_CHANGE_PWD: return handle_change_pwd(ev);
    case SCREEN_READER_LIST: return handle_reader_list(ev);
    case SCREEN_LOAN_MANAGE: return handle_loan_manage(ev);
    case SCREEN_RESERVATION_MANAGE: return handle_reservation_manage(ev);
    case SCREEN_FINE_MANAGE: return handle_fine_manage(ev);
    default: return false;
    }
}

/* =====================================================================
 * M2：图书管理 / 字典管理 / 修改密码
 * ===================================================================== */

static unsigned short ui_invert(unsigned short a) {
    return (unsigned short)(((a & 0x0F) << 4) | ((a & 0xF0) >> 4));
}

static const char *ui_book_status(BookStatus s) {
    switch (s) {
    case BOOK_STATUS_SUSPENDED: return "停借";
    case BOOK_STATUS_DELETED: return "注销";
    default: return "在架";
    }
}

/* ---- 字典名称 ⇄ ID（名称在字典表中唯一） ---- */
static EntityId author_id_by_name(const char *name) {
    const Author *arr;
    size_t n, i;
    if (!name || !*name) return 0;
    arr = repo_authors(&n);
    for (i = 0; i < n; i++)
        if (strcmp(arr[i].name, name) == 0) return arr[i].id;
    return 0;
}
static EntityId publisher_id_by_name(const char *name) {
    const Publisher *arr;
    size_t n, i;
    if (!name || !*name) return 0;
    arr = repo_publishers(&n);
    for (i = 0; i < n; i++)
        if (strcmp(arr[i].name, name) == 0) return arr[i].id;
    return 0;
}
static EntityId category_id_by_name(const char *name) {
    const Category *arr;
    size_t n, i;
    if (!name || !*name) return 0;
    arr = repo_categories(&n);
    for (i = 0; i < n; i++)
        if (strcmp(arr[i].name, name) == 0) return arr[i].id;
    return 0;
}
static const char *author_name_by_id(EntityId id) {
    const Author *a = repo_find_author(id);
    return a ? a->name : "(未知)";
}
static const char *publisher_name_by_id(EntityId id) {
    const Publisher *p = repo_find_publisher(id);
    return p ? p->name : "(未知)";
}
static const char *category_name_by_id(EntityId id) {
    const Category *c = repo_find_category(id);
    return c ? c->name : "(未知)";
}

/* ================= 图书检索列表 ================= */

static void bs_run_search(void) {
    s_bs_count = 0;
    if (search_books((BookSearchField)s_bs_field, s_bs_keyword,
                     s_bs_res, BS_MAX, &s_bs_count) != RES_OK) {
        s_bs_count = 0;
        str_copy(s_msg, sizeof s_msg, "检索失败：内存不足");
    }
    s_bs_rebuild = true;
}

static void bs_rebuild_rows(void) {
    char **rows;
    size_t i;
    if (!s_bs_rebuild) return;
    s_bs_rebuild = false;
    if (s_bs_count == 0) {
        widget_list_set_items(g_book_list, NULL, 0);
        return;
    }
    rows = malloc(s_bs_count * sizeof(char *));
    if (!rows) {
        widget_list_set_items(g_book_list, NULL, 0);
        return;
    }
    for (i = 0; i < s_bs_count; i++) {
        const Book *b = s_bs_res[i];
        char *r = malloc(128);
        char idbuf[16], copies[24];
        const char *st = ui_book_status(b->status);
        int av = b->status == BOOK_STATUS_SUSPENDED ? 0 : b->available_copies;
        size_t j;
        if (!r) {
            for (j = 0; j < i; j++) free(rows[j]);
            free(rows);
            widget_list_set_items(g_book_list, NULL, 0);
            return;
        }
        snprintf(idbuf, sizeof idbuf, "%d", b->id);
        snprintf(copies, sizeof copies, "%d/%d", av, b->total_copies);
        r[0] = '\0';
        ui_append(r, 128, idbuf, 4);
        ui_append(r, 128, b->isbn, 18);
        ui_append(r, 128, b->title, 26);
        ui_append(r, 128, copies, 9);
        ui_append(r, 128, b->location, 10);
        ui_append(r, 128, st, 6);
        rows[i] = r;
    }
    widget_list_set_items(g_book_list, (const char *const *)rows, s_bs_count);
    for (i = 0; i < s_bs_count; i++) free(rows[i]);
    free(rows);
    widget_list_set_sel(g_book_list, 0);
}

static void draw_book_list(void) {
    static const char *fname[] = {"ISBN", "书名", "作者", "分类"};
    char buf[128], hdr[160], title[64];
    int i, bar_w = 74, sx, panel_y = 3, panel_h, row0, by;
    bool admin = g_session.role == ROLE_ADMIN;

    bs_rebuild_rows();

    if (bar_w > s_cols - 4) bar_w = s_cols - 4;
    sx = (s_cols - bar_w) / 2;
    if (sx < 1) sx = 1;
    L_sx = sx;

    /* ---- 检索条 ---- */
    console_set_color(COL_HINT);
    console_write_at(sx, 1, "检索字段");
    console_set_color(COL_BODY);
    for (i = 0; i < 4; i++) {
        snprintf(buf, sizeof buf, "[%d %s]", i + 1, fname[i]);
        console_set_color(s_bs_field == i ? COL_ACCENT : COL_BODY);
        console_write_at(sx + 10 + i * 9, 1, buf);
    }
    console_set_color(COL_HINT);
    console_write_at(sx + 47, 1, "关键字");
    console_set_color(COL_BODY);
    snprintf(buf, sizeof buf, "[%-16s]", s_bs_keyword);
    if (s_bs_typing) console_set_color(ui_invert(COL_BODY));
    console_write_at(sx + 55, 1, buf);
    console_set_color(COL_BODY);

    /* ---- 列表 + 表格 ---- */
    panel_h = s_rows - panel_y - 4;
    if (panel_h < 6) panel_h = 6;
    ui_panel(2, panel_y, s_cols - 4, panel_h, NULL);
    snprintf(title, sizeof title, " 图书列表 · %zu 条 ", s_bs_count);
    console_set_color(COL_TITLE);
    console_write_at(4, panel_y, title);
    console_set_color(COL_BODY);

    hdr[0] = '\0';
    ui_append(hdr, sizeof hdr, "ID", 4);
    ui_append(hdr, sizeof hdr, "ISBN", 18);
    ui_append(hdr, sizeof hdr, "书名", 26);
    ui_append(hdr, sizeof hdr, "可借", 9);
    ui_append(hdr, sizeof hdr, "位置", 10);
    ui_append(hdr, sizeof hdr, "状态", 6);
    console_set_color(COL_ACCENT);
    console_write_at(4, panel_y + 1, hdr);
    console_set_color(COL_BODY);
    ui_sep(3, panel_y + 2, s_cols - 6);

    row0 = panel_y + 3;
    widget_set_rect(g_book_list, 4, row0, s_cols - 8, panel_h - 4);
    widget_draw(g_book_list);

    draw_msg(4, s_rows - 4);

    /* ---- 操作按钮 ---- */
    by = s_rows - 3;
    if (admin) {
        widget_set_rect(g_ba_new, 4, by, 12, 1);
        widget_set_rect(g_ba_view, 18, by, 12, 1);
        widget_set_rect(g_ba_edit, 32, by, 12, 1);
        widget_set_rect(g_ba_del, 46, by, 12, 1);
        widget_set_rect(g_ba_sus, 60, by, 16, 1);
        widget_set_rect(g_ba_back, 78, by, 12, 1);
        widget_draw(g_ba_new);
        widget_draw(g_ba_view);
        widget_draw(g_ba_edit);
        widget_draw(g_ba_del);
        widget_draw(g_ba_sus);
    } else {
        widget_set_rect(g_ba_view, 4, by, 12, 1);
        widget_set_rect(g_ba_back, 18, by, 12, 1);
        widget_draw(g_ba_view);
        console_set_color(COL_HINT);
        console_write_at(34, by, "只读检索（借阅功能将在 M3 提供）");
        console_set_color(COL_BODY);
    }
    widget_draw(g_ba_back);
    draw_hint_bar("↑↓ 选择 · Enter 详情 · A 新增 · E 编辑 · D 删除 · S 停借/恢复");
}

static const Book *bl_selected(void) {
    int sel = widget_list_sel(g_book_list);
    if (sel < 0 || (size_t)sel >= s_bs_count) return NULL;
    return s_bs_res[sel];
}

static void bl_add(void) {
    memset(&s_bf, 0, sizeof s_bf);
    s_bf.mode = BF_ADD;
    s_bf.focus = 0;
    s_msg[0] = '\0';
    screen_goto(SCREEN_BOOK_EDIT);
}

static void bl_open(int mode) { /* BF_VIEW / BF_EDIT */
    const Book *b = bl_selected();
    if (!b) {
        str_copy(s_msg, sizeof s_msg, "请先选中一本图书");
        return;
    }
    memset(&s_bf, 0, sizeof s_bf);
    s_bf.mode = (BfMode)mode;
    s_bf.id = b->id;
    str_copy(s_bf.isbn, sizeof s_bf.isbn, b->isbn);
    str_copy(s_bf.title, sizeof s_bf.title, b->title);
    str_copy(s_bf.author, sizeof s_bf.author, author_name_by_id(b->author_id));
    str_copy(s_bf.publisher, sizeof s_bf.publisher, publisher_name_by_id(b->publisher_id));
    str_copy(s_bf.category, sizeof s_bf.category, category_name_by_id(b->category_id));
    str_copy(s_bf.pub_date, sizeof s_bf.pub_date, b->pub_date);
    snprintf(s_bf.price, sizeof s_bf.price, "%g", b->price);
    snprintf(s_bf.copies, sizeof s_bf.copies, "%d", b->total_copies);
    str_copy(s_bf.location, sizeof s_bf.location, b->location);
    str_copy(s_bf.desc, sizeof s_bf.desc, b->description);
    s_msg[0] = '\0';
    screen_goto(SCREEN_BOOK_EDIT);
}

static void bl_toggle_suspend(void) {
    const Book *b = bl_selected();
    Result r;
    if (!b) {
        str_copy(s_msg, sizeof s_msg, "请先选中一本图书");
        return;
    }
    r = (b->status == BOOK_STATUS_SUSPENDED) ? book_resume(b->id) : book_suspend(b->id);
    if (r == RES_OK) {
        snprintf(s_msg, sizeof s_msg, "%s（id=%d）",
                 b->status == BOOK_STATUS_SUSPENDED ? "已恢复在架" : "已停借", b->id);
    } else {
        snprintf(s_msg, sizeof s_msg, "操作失败 (code=%d)", r);
    }
    bs_run_search();
}

static void bl_delete(void) { /* 二次确认 */
    const Book *b = bl_selected();
    Result r;
    if (!b) {
        str_copy(s_msg, sizeof s_msg, "请先选中一本图书");
        return;
    }
    if (s_pending_del == b->id && s_pending_kind == 0) {
        s_pending_del = 0;
        r = book_delete(b->id);
        if (r == RES_OK) {
            str_copy(s_msg, sizeof s_msg, "图书已删除（状态=注销）");
        } else if (r == RES_ERR) {
            str_copy(s_msg, sizeof s_msg, "无法删除：该图书有未还副本，可改为「停借」");
        } else {
            snprintf(s_msg, sizeof s_msg, "删除失败 (code=%d)", r);
        }
        bs_run_search();
    } else {
        s_pending_del = b->id;
        s_pending_kind = 0;
        str_copy(s_msg, sizeof s_msg, "再按一次 D/删除 确认；不想删除就选其他行或按其他键");
    }
}

static bool handle_book_list(const InputEvent *ev) {
    int sel;
    bool admin = g_session.role == ROLE_ADMIN;

    if (ev->type == EV_KEY && ev->key_down) {
        if (s_bs_typing) {
            if (ev->vk == VK_RETURN) { s_bs_typing = false; bs_run_search(); return true; }
            if (ev->vk == VK_BACK) { utf8_backspace(s_bs_keyword); return true; }
            if (ev->ch >= 0x20) { field_append(s_bs_keyword, sizeof s_bs_keyword, ev->ch); return true; }
            return false;
        }
        sel = widget_list_sel(g_book_list);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_book_list, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_book_list, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(g_book_list, 0); return true;
        case VK_END: widget_list_set_sel(g_book_list, (int)s_bs_count - 1); return true;
        case VK_PRIOR: widget_list_set_sel(g_book_list, sel - 12); return true;
        case VK_NEXT: widget_list_set_sel(g_book_list, sel + 12); return true;
        case VK_RETURN: bl_open(BF_VIEW); return true;
        default: break;
        }
        if (ev->ch >= '1' && ev->ch <= '4') { s_bs_field = ev->ch - '1'; bs_run_search(); return true; }
        if (admin && (ev->ch == 'a' || ev->ch == 'A')) { bl_add(); return true; }
        if (ev->ch == 'v' || ev->ch == 'V') { bl_open(BF_VIEW); return true; }
        if (admin && (ev->ch == 'e' || ev->ch == 'E')) { bl_open(BF_EDIT); return true; }
        if (admin && (ev->ch == 'd' || ev->ch == 'D')) { bl_delete(); return true; }
        if (admin && (ev->ch == 's' || ev->ch == 'S')) { bl_toggle_suspend(); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_book_list, widget_list_sel(g_book_list) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        Widget *btns[6] = {g_ba_new, g_ba_view, g_ba_edit, g_ba_del, g_ba_sus, g_ba_back};
        int lx, ly, lw, lh;
        bool changed = hover_changed(ev, btns, 6);
        if (left_press_edge(ev)) {
            if (ev->y == 1 && ev->x >= L_sx + 10 && ev->x < L_sx + 46) {
                int f = (ev->x - (L_sx + 10)) / 9;
                if (f >= 0 && f <= 3) { s_bs_field = f; bs_run_search(); return true; }
            }
            if (ev->y == 1 && ev->x >= L_sx + 55 && ev->x < L_sx + 73) {
                s_bs_typing = true;
                return true;
            }
            widget_get_rect(g_book_list, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                int row = ev->y - ly;
                if (row < (int)s_bs_count) widget_list_set_sel(g_book_list, row);
                return true;
            }
            if (admin && widget_hit(g_ba_new, ev->x, ev->y)) bl_add();
            else if (widget_hit(g_ba_view, ev->x, ev->y)) bl_open(BF_VIEW);
            else if (admin && widget_hit(g_ba_edit, ev->x, ev->y)) bl_open(BF_EDIT);
            else if (admin && widget_hit(g_ba_del, ev->x, ev->y)) bl_delete();
            else if (admin && widget_hit(g_ba_sus, ev->x, ev->y)) bl_toggle_suspend();
            else if (widget_hit(g_ba_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* ================= 图书表单（新增/编辑/详情） ================= */

static char *bf_field(int i) {
    switch (i) {
    case 0: return s_bf.isbn;
    case 1: return s_bf.title;
    case 2: return s_bf.author;
    case 3: return s_bf.publisher;
    case 4: return s_bf.category;
    case 5: return s_bf.pub_date;
    case 6: return s_bf.price;
    case 7: return s_bf.copies;
    case 8: return s_bf.location;
    default: return s_bf.desc;
    }
}

static size_t bf_cap(int i) {
    switch (i) {
    case 0: return sizeof s_bf.isbn;
    case 1: return sizeof s_bf.title;
    case 5: return sizeof s_bf.pub_date;
    case 6: return sizeof s_bf.price;
    case 7: return sizeof s_bf.copies;
    case 8: return sizeof s_bf.location;
    default: return sizeof s_bf.desc;
    }
}

static void bf_save(void) {
    Book b;
    EntityId aid, pid, cid;
    double price;
    int copies;
    Result r;

    if (str_empty(s_bf.isbn) || str_empty(s_bf.title)) {
        str_copy(s_msg, sizeof s_msg, "ISBN 与书名不能为空");
        return;
    }
    aid = author_id_by_name(s_bf.author);
    if (!aid) {
        str_copy(s_msg, sizeof s_msg, "作者「不存在」或为空：请先在「字典管理-作者」中添加");
        return;
    }
    pid = publisher_id_by_name(s_bf.publisher);
    if (!pid) {
        str_copy(s_msg, sizeof s_msg, "出版社不存在：请先在「字典管理-出版社」中添加");
        return;
    }
    cid = category_id_by_name(s_bf.category);
    if (!cid) {
        str_copy(s_msg, sizeof s_msg, "分类不存在：请先在「字典管理-分类」中添加");
        return;
    }
    if (!str_empty(s_bf.pub_date) && !valid_date_str(s_bf.pub_date)) {
        str_copy(s_msg, sizeof s_msg, "出版日期格式应为 YYYY-MM-DD（可留空）");
        return;
    }
    if (!valid_number_str(s_bf.price, &price)) {
        str_copy(s_msg, sizeof s_msg, "定价必须是大于等于 0 的数字");
        return;
    }
    if (!valid_digits(s_bf.copies) || (copies = atoi(s_bf.copies)) < 1) {
        str_copy(s_msg, sizeof s_msg, "馆藏总数必须是 ≥1 的整数");
        return;
    }

    memset(&b, 0, sizeof b);
    str_copy(b.isbn, sizeof b.isbn, s_bf.isbn);
    str_copy(b.title, sizeof b.title, s_bf.title);
    b.author_id = aid;
    b.publisher_id = pid;
    b.category_id = cid;
    str_copy(b.pub_date, sizeof b.pub_date, s_bf.pub_date);
    b.price = price;
    b.total_copies = copies;
    str_copy(b.location, sizeof b.location, s_bf.location);
    str_copy(b.description, sizeof b.description, s_bf.desc);

    if (s_bf.mode == BF_ADD) {
        b.status = BOOK_STATUS_ON_SHELF;
        b.available_copies = copies;
        r = book_add(&b, NULL);
    } else {
        Book *cur = repo_find_book(s_bf.id);
        if (!cur) {
            str_copy(s_msg, sizeof s_msg, "原图书不存在，可能已被删除");
            return;
        }
        b.id = s_bf.id;
        b.status = cur->status;
        str_copy(b.created_at, sizeof b.created_at, cur->created_at);
        r = book_update(&b);
    }

    if (r == RES_OK) {
        str_copy(s_msg, sizeof s_msg, s_bf.mode == BF_ADD ? "图书新增成功" : "图书已保存");
        screen_back(); /* 回图书列表，on_enter 自动刷新 */
    } else if (r == RES_ERR_DUP) {
        str_copy(s_msg, sizeof s_msg, "保存失败：ISBN 已存在");
    } else if (r == RES_ERR_LIMIT) {
        str_copy(s_msg, sizeof s_msg, "馆藏总数不能小于已借出副本数");
    } else {
        snprintf(s_msg, sizeof s_msg, "保存失败 (code=%d)", r);
    }
}

static void draw_book_form(void) {
    static const char *labels[] = {"ISBN", "书名", "作者", "出版社", "分类",
                                   "出版日期", "定价", "馆藏总数", "馆藏位置", "简介"};
    char buf[160];
    const char *title = s_bf.mode == BF_ADD ? "新增图书"
                        : s_bf.mode == BF_EDIT ? "编辑图书" : "图书详情";
    bool ro = s_bf.mode == BF_VIEW;
    int pw = 76, ph, px, py = 2, i;

    if (pw > s_cols - 6) pw = s_cols - 6;
    px = (s_cols - pw) / 2;
    ph = s_rows - 4;
    if (ph < 18) ph = 18;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;
    L_fx = px + 16;
    L_fw = pw - 20;
    L_fy = py + 4;
    L_msg_y = py + ph - 2;

    ui_panel(px, py, pw, ph, title);
    if (!ro) {
        console_set_color(COL_HINT);
        console_write_at(px + 3, py + 2, "作者 / 出版社 / 分类按名称匹配，须先在「字典管理」中创建");
        console_set_color(COL_BODY);
    }
    for (i = 0; i < 10; i++) {
        int row = L_fy + i;
        console_set_color(!ro && s_bf.focus == i ? COL_ACCENT : COL_BODY);
        console_write_at(px + 4, row, labels[i]);
        console_set_color(COL_BODY);
        ui_pad(buf, sizeof buf, bf_field(i), L_fw);
        if (!ro && s_bf.focus != i) console_set_color(COL_HINT);
        console_write_at(L_fx, row, buf);
        console_set_color(COL_BODY);
    }
    ui_sep(px + 1, py + ph - 5, pw - 2);
    if (ro) {
        widget_set_rect(g_bf_back, (s_cols - 14) / 2, py + ph - 4, 14, 1);
        widget_draw(g_bf_back);
    } else {
        widget_set_rect(g_bf_save, px + 18, py + ph - 4, 14, 1);
        widget_set_rect(g_bf_cancel, px + pw - 32, py + ph - 4, 14, 1);
        widget_draw(g_bf_save);
        widget_draw(g_bf_cancel);
    }
    draw_msg(px + 3, L_msg_y);
    draw_hint_bar(ro ? "Enter/Esc 返回列表" : "Tab 切换字段 · Enter 下一项/保存 · Esc 取消");
}

static bool handle_book_form(const InputEvent *ev) {
    bool ro = s_bf.mode == BF_VIEW;
    if (ev->type == EV_KEY && ev->key_down) {
        if (ro) return false;
        if (ev->vk == VK_TAB) { s_bf.focus = (s_bf.focus + 1) % 10; return true; }
        if (ev->vk == VK_RETURN) {
            if (s_bf.focus == 9) bf_save();
            else s_bf.focus++;
            return true;
        }
        if (ev->vk == VK_BACK) { utf8_backspace(bf_field(s_bf.focus)); return true; }
        if (ev->ch >= 0x20) { field_append(bf_field(s_bf.focus), bf_cap(s_bf.focus), ev->ch); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE) {
        if (ro) {
            bool changed = hover_changed(ev, (Widget *[]){g_bf_back}, 1);
            if (left_press_edge(ev) && widget_hit(g_bf_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
            return changed;
        }
        {
            bool changed = hover_changed(ev, (Widget *[]){g_bf_save, g_bf_cancel}, 2);
            if (left_press_edge(ev)) {
                if (ev->y >= L_fy && ev->y < L_fy + 10 &&
                    ev->x >= L_fx - 4 && ev->x < L_fx + L_fw) {
                    s_bf.focus = ev->y - L_fy;
                    return true;
                }
                if (widget_hit(g_bf_save, ev->x, ev->y)) { bf_save(); return true; }
                if (widget_hit(g_bf_cancel, ev->x, ev->y)) {
                    s_msg[0] = '\0';
                    screen_back();
                    return true;
                }
            }
            return changed;
        }
    }
    return false;
}

/* ================= 字典管理 ================= */

static int dict_count(void) {
    switch (s_dict.kind) {
    case DICT_AUTHOR: return 4;
    case DICT_PUB: return 3;
    default: return 1;
    }
}

static const char *dict_label(int i) {
    if (s_dict.kind == DICT_AUTHOR)
        return i == 0 ? "姓名" : i == 1 ? "出生年份" : i == 2 ? "国籍" : "简介";
    if (s_dict.kind == DICT_PUB)
        return i == 0 ? "名称" : i == 1 ? "地址" : "电话";
    return "名称";
}

static char *dict_val(int i) {
    if (s_dict.kind == DICT_AUTHOR)
        return i == 0 ? s_dict.name : i == 1 ? s_dict.opt1 : i == 2 ? s_dict.opt2 : s_dict.desc;
    if (s_dict.kind == DICT_PUB)
        return i == 0 ? s_dict.name : i == 1 ? s_dict.opt1 : s_dict.opt2;
    return s_dict.name;
}

static size_t dict_cap(int i) {
    if (s_dict.kind == DICT_AUTHOR)
        return i == 0 ? sizeof s_dict.name : i == 1 ? sizeof s_dict.opt1
                : i == 2 ? sizeof s_dict.opt2 : sizeof s_dict.desc;
    if (s_dict.kind == DICT_PUB)
        return i == 0 ? sizeof s_dict.name : i == 1 ? sizeof s_dict.opt1 : sizeof s_dict.opt2;
    return sizeof s_dict.name;
}

static const char *dict_kind_name(void) {
    return s_dict.kind == DICT_AUTHOR ? "作者"
           : s_dict.kind == DICT_PUB ? "出版社" : "分类";
}

static void dict_rebuild_rows(void) {
    char **rows = NULL;
    size_t i, n = 0;
    if (!s_dict_rebuild) return;
    s_dict_rebuild = false;
    free(s_dict_ids);
    s_dict_ids = NULL;
    s_dict_ids_count = 0;

    if (s_dict.kind == DICT_AUTHOR) {
        const Author *arr = repo_authors(&n);
        if (n) {
            rows = malloc(n * sizeof(char *));
            s_dict_ids = malloc(n * sizeof(EntityId));
            if (rows && s_dict_ids) {
                for (i = 0; i < n; i++) {
                    char *r = malloc(96);
                    if (!r) break;
                    snprintf(r, 96, "%3d %-20.20s %-12.12s %d", arr[i].id,
                             arr[i].name, arr[i].nationality, arr[i].birth_year);
                    rows[i] = r;
                    s_dict_ids[i] = arr[i].id;
                }
            }
        }
    } else if (s_dict.kind == DICT_PUB) {
        const Publisher *arr = repo_publishers(&n);
        if (n) {
            rows = malloc(n * sizeof(char *));
            s_dict_ids = malloc(n * sizeof(EntityId));
            if (rows && s_dict_ids) {
                for (i = 0; i < n; i++) {
                    char *r = malloc(96);
                    if (!r) break;
                    snprintf(r, 96, "%3d %-24.24s %.60s", arr[i].id,
                             arr[i].name, arr[i].address);
                    rows[i] = r;
                    s_dict_ids[i] = arr[i].id;
                }
            }
        }
    } else {
        const Category *arr = repo_categories(&n);
        if (n) {
            rows = malloc(n * sizeof(char *));
            s_dict_ids = malloc(n * sizeof(EntityId));
            if (rows && s_dict_ids) {
                for (i = 0; i < n; i++) {
                    char *r = malloc(96);
                    if (!r) break;
                    snprintf(r, 96, "%3d %s", arr[i].id, arr[i].name);
                    rows[i] = r;
                    s_dict_ids[i] = arr[i].id;
                }
            }
        }
    }
    if (rows) {
        size_t ok = 0;
        while (ok < s_dict_ids_count && rows[ok]) ok++;
        widget_list_set_items(g_dict_list, (const char *const *)rows, ok);
        for (i = 0; i < ok; i++) free(rows[i]);
        free(rows);
        widget_list_set_sel(g_dict_list, 0);
    } else {
        widget_list_set_items(g_dict_list, NULL, 0);
    }
}

static void dict_set_kind(DictKind k) {
    if (s_dict.kind == k) return;
    s_dict.kind = k;
    s_dict_rebuild = true;
}

static EntityId dict_selected_id(void) {
    int sel = widget_list_sel(g_dict_list);
    if (sel < 0 || (size_t)sel >= s_dict_ids_count) return 0;
    return s_dict_ids[sel];
}

static void dict_enter_add(void) {
    memset(&s_dict.name, 0, sizeof s_dict.name);
    memset(&s_dict.opt1, 0, sizeof s_dict.opt1);
    memset(&s_dict.opt2, 0, sizeof s_dict.opt2);
    memset(&s_dict.desc, 0, sizeof s_dict.desc);
    s_dict.edit_id = 0;
    s_dict.focus = 0;
    s_dict.mode = 1;
    s_msg[0] = '\0';
}

static void dict_enter_edit(void) {
    EntityId id = dict_selected_id();
    if (!id) {
        str_copy(s_msg, sizeof s_msg, "请先选中一项");
        return;
    }
    s_dict.edit_id = id;
    memset(&s_dict.name, 0, sizeof s_dict.name);
    memset(&s_dict.opt1, 0, sizeof s_dict.opt1);
    memset(&s_dict.opt2, 0, sizeof s_dict.opt2);
    memset(&s_dict.desc, 0, sizeof s_dict.desc);
    if (s_dict.kind == DICT_AUTHOR) {
        const Author *a = repo_find_author(id);
        if (!a) return;
        str_copy(s_dict.name, sizeof s_dict.name, a->name);
        snprintf(s_dict.opt1, sizeof s_dict.opt1, "%d", a->birth_year);
        str_copy(s_dict.opt2, sizeof s_dict.opt2, a->nationality);
        str_copy(s_dict.desc, sizeof s_dict.desc, a->description);
    } else if (s_dict.kind == DICT_PUB) {
        const Publisher *p = repo_find_publisher(id);
        if (!p) return;
        str_copy(s_dict.name, sizeof s_dict.name, p->name);
        str_copy(s_dict.opt1, sizeof s_dict.opt1, p->address);
        str_copy(s_dict.opt2, sizeof s_dict.opt2, p->phone);
    } else {
        const Category *c = repo_find_category(id);
        if (!c) return;
        str_copy(s_dict.name, sizeof s_dict.name, c->name);
    }
    s_dict.focus = 0;
    s_dict.mode = 1;
    s_msg[0] = '\0';
}

static void dict_save(void) {
    Result r;
    const char *kindtxt = dict_kind_name();
    if (str_empty(s_dict.name)) {
        snprintf(s_msg, sizeof s_msg, "%s名称不能为空", kindtxt);
        return;
    }
    if (s_dict.kind == DICT_AUTHOR) {
        Author a;
        memset(&a, 0, sizeof a);
        str_copy(a.name, sizeof a.name, s_dict.name);
        a.birth_year = valid_digits(s_dict.opt1) ? atoi(s_dict.opt1) : 0;
        str_copy(a.nationality, sizeof a.nationality, s_dict.opt2);
        str_copy(a.description, sizeof a.description, s_dict.desc);
        if (s_dict.edit_id) { a.id = s_dict.edit_id; r = author_update(&a); }
        else r = author_add(&a, NULL);
    } else if (s_dict.kind == DICT_PUB) {
        Publisher p;
        memset(&p, 0, sizeof p);
        str_copy(p.name, sizeof p.name, s_dict.name);
        str_copy(p.address, sizeof p.address, s_dict.opt1);
        str_copy(p.phone, sizeof p.phone, s_dict.opt2);
        if (s_dict.edit_id) { p.id = s_dict.edit_id; r = publisher_update(&p); }
        else r = publisher_add(&p, NULL);
    } else {
        Category c;
        memset(&c, 0, sizeof c);
        str_copy(c.name, sizeof c.name, s_dict.name);
        if (s_dict.edit_id) { c.id = s_dict.edit_id; r = category_update(&c); }
        else r = category_add(&c, NULL);
    }
    if (r == RES_OK) {
        snprintf(s_msg, sizeof s_msg, "%s已保存", kindtxt);
        s_dict.mode = 0;
        s_dict_rebuild = true;
    } else if (r == RES_ERR_DUP) {
        snprintf(s_msg, sizeof s_msg, "%s名称已存在", kindtxt);
    } else {
        snprintf(s_msg, sizeof s_msg, "保存失败 (code=%d)", r);
    }
}

static void dict_delete(void) { /* 二次确认 */
    EntityId id = dict_selected_id();
    Result r;
    if (!id) {
        str_copy(s_msg, sizeof s_msg, "请先选中一项");
        return;
    }
    if (s_pending_del == id && s_pending_kind == 1 + (int)s_dict.kind) {
        s_pending_del = 0;
        if (s_dict.kind == DICT_AUTHOR) r = author_delete(id);
        else if (s_dict.kind == DICT_PUB) r = publisher_delete(id);
        else r = category_delete(id);
        if (r == RES_OK) {
            snprintf(s_msg, sizeof s_msg, "%s「%.60s」已删除", dict_kind_name(), s_dict.name);
        } else if (r == RES_ERR) {
            str_copy(s_msg, sizeof s_msg, "无法删除：该字典项正被图书引用");
        } else {
            snprintf(s_msg, sizeof s_msg, "删除失败 (code=%d)", r);
        }
        s_dict_rebuild = true;
    } else {
        s_pending_del = id;
        s_pending_kind = 1 + (int)s_dict.kind;
        str_copy(s_msg, sizeof s_msg, "再按一次 D/删除 确认删除该项");
    }
}

static void draw_dict(void) {
    char buf[160];
    int pw = 58, ph, px, py = 2, i;
    if (pw > s_cols - 6) pw = s_cols - 6;
    px = (s_cols - pw) / 2;
    ph = s_rows - 5;
    if (ph < 12) ph = 12;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;

    /* 页签 */
    widget_set_rect(g_dk_auth, px, 0, 12, 1);
    widget_set_rect(g_dk_pub, px + 13, 0, 12, 1);
    widget_set_rect(g_dk_cat, px + 26, 0, 12, 1);
    widget_set_selected(g_dk_auth, s_dict.kind == DICT_AUTHOR);
    widget_set_selected(g_dk_pub, s_dict.kind == DICT_PUB);
    widget_set_selected(g_dk_cat, s_dict.kind == DICT_CAT);
    widget_draw(g_dk_auth);
    widget_draw(g_dk_pub);
    widget_draw(g_dk_cat);

    if (s_dict.mode == 0) { /* ---- 列表 ---- */
        int by;
        dict_rebuild_rows();
        snprintf(buf, sizeof buf, "字典管理 · %s", dict_kind_name());
        ui_panel(px, py, pw, ph, buf);
        widget_set_rect(g_dict_list, px + 2, py + 2, pw - 4, ph - 4);
        widget_draw(g_dict_list);
        by = py + ph - 2;
        widget_set_rect(g_da_new, px + 2, by, 12, 1);
        widget_set_rect(g_da_edit, px + 16, by, 12, 1);
        widget_set_rect(g_da_del, px + 30, by, 12, 1);
        widget_set_rect(g_da_back, px + pw - 14, by, 12, 1);
        widget_draw(g_da_new);
        widget_draw(g_da_edit);
        widget_draw(g_da_del);
        widget_draw(g_da_back);
        draw_msg(px, py + ph + 1);
        draw_hint_bar("1/2/3 切换字典 · A 新增 · E 编辑 · D 删除 · Esc 返回");
    } else { /* ---- 表单 ---- */
        snprintf(buf, sizeof buf, s_dict.edit_id ? "编辑%s" : "新增%s", dict_kind_name());
        ui_panel(px, py, pw, ph, buf);
        L_fx = px + 16;
        L_fw = pw - 20;
        L_fy = py + 3;
        L_msg_y = py + ph - 2;
        for (i = 0; i < dict_count(); i++) {
            int row = L_fy + i;
            console_set_color(s_dict.focus == i ? COL_ACCENT : COL_BODY);
            console_write_at(px + 4, row, dict_label(i));
            console_set_color(COL_BODY);
            ui_pad(buf, sizeof buf, dict_val(i), L_fw);
            if (s_dict.focus != i) console_set_color(COL_HINT);
            console_write_at(L_fx, row, buf);
            console_set_color(COL_BODY);
        }
        ui_sep(px + 1, py + ph - 5, pw - 2);
        widget_set_rect(g_df_save, px + 18, py + ph - 4, 14, 1);
        widget_set_rect(g_df_cancel, px + pw - 32, py + ph - 4, 14, 1);
        widget_draw(g_df_save);
        widget_draw(g_df_cancel);
        draw_msg(px + 3, L_msg_y);
        draw_hint_bar("Tab 切换字段 · Enter 下一项/保存 · Esc 取消");
    }
}

static bool handle_dict(const InputEvent *ev) {
    int i, n;
    if (s_dict.mode == 1) { /* 表单 */
        n = dict_count();
        if (ev->type == EV_KEY && ev->key_down) {
            if (ev->vk == VK_TAB) { s_dict.focus = (s_dict.focus + 1) % n; return true; }
            if (ev->vk == VK_RETURN) {
                if (s_dict.focus == n - 1) dict_save();
                else s_dict.focus++;
                return true;
            }
            if (ev->vk == VK_BACK) { utf8_backspace(dict_val(s_dict.focus)); return true; }
            if (ev->ch >= 0x20) {
                field_append(dict_val(s_dict.focus), dict_cap(s_dict.focus), ev->ch);
                return true;
            }
            return false;
        }
        if (ev->type == EV_MOUSE) {
            bool changed = hover_changed(ev, (Widget *[]){g_df_save, g_df_cancel}, 2);
            if (left_press_edge(ev)) {
                if (ev->y >= L_fy && ev->y < L_fy + n &&
                    ev->x >= L_fx - 4 && ev->x < L_fx + L_fw) {
                    s_dict.focus = ev->y - L_fy;
                    return true;
                }
                if (widget_hit(g_df_save, ev->x, ev->y)) { dict_save(); return true; }
                if (widget_hit(g_df_cancel, ev->x, ev->y)) {
                    s_dict.mode = 0;
                    s_dict_rebuild = true;
                    return true;
                }
            }
            return changed;
        }
        return false;
    }

    /* 列表模式 */
    if (ev->type == EV_KEY && ev->key_down) {
        int sel = widget_list_sel(g_dict_list);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_dict_list, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_dict_list, sel + 1); return true;
        case VK_RETURN: dict_enter_edit(); return true;
        default: break;
        }
        if (ev->ch >= '1' && ev->ch <= '3') { dict_set_kind((DictKind)(ev->ch - '1')); return true; }
        if (ev->ch == 'a' || ev->ch == 'A') { dict_enter_add(); return true; }
        if (ev->ch == 'e' || ev->ch == 'E') { dict_enter_edit(); return true; }
        if (ev->ch == 'd' || ev->ch == 'D') { dict_delete(); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_dict_list, widget_list_sel(g_dict_list) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        Widget *top[3] = {g_dk_auth, g_dk_pub, g_dk_cat};
        Widget *acts[4] = {g_da_new, g_da_edit, g_da_del, g_da_back};
        bool changed = hover_changed(ev, top, 3);
        if (hover_changed(ev, acts, 4)) changed = true;
        if (left_press_edge(ev)) {
            for (i = 0; i < 3; i++)
                if (widget_hit(top[i], ev->x, ev->y)) { dict_set_kind((DictKind)i); return true; }
            widget_get_rect(g_dict_list, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                widget_list_set_sel(g_dict_list, ev->y - ly);
                return true;
            }
            if (widget_hit(g_da_new, ev->x, ev->y)) dict_enter_add();
            else if (widget_hit(g_da_edit, ev->x, ev->y)) dict_enter_edit();
            else if (widget_hit(g_da_del, ev->x, ev->y)) dict_delete();
            else if (widget_hit(g_da_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* ================= 修改密码 ================= */

static void pwd_save(void) {
    Result r;
    if (strcmp(s_pwd.new_pwd, s_pwd.confirm) != 0) {
        str_copy(s_msg, sizeof s_msg, "两次输入的新密码不一致");
        return;
    }
    r = auth_change_password(s_pwd.old_pwd, s_pwd.new_pwd);
    if (r == RES_OK) {
        str_copy(s_msg, sizeof s_msg, "密码修改成功，请牢记新密码");
        screen_back();
    } else if (r == RES_ERR_INVALID) {
        str_copy(s_msg, sizeof s_msg, "修改失败：旧密码错误，或新密码少于 6 位");
    } else {
        snprintf(s_msg, sizeof s_msg, "修改失败 (code=%d)", r);
    }
}

static void draw_change_pwd(void) {
    static const char *labels[] = {"旧密码", "新密码", "确认新密码"};
    char buf[80];
    int pw = 46, ph = 11, px, py = 4, i;
    if (pw > s_cols - 8) pw = s_cols - 8;
    px = (s_cols - pw) / 2;

    L_px = px;
    L_py = py;
    L_pw = pw;
    L_ph = ph;
    L_fx = px + 14;
    L_fw = pw - 18;
    L_fy = py + 3;
    L_msg_y = py + ph - 2;

    ui_panel(px, py, pw, ph, "修改密码");
    console_set_color(COL_HINT);
    console_write_at(px + 3, py + 1, "新密码不少于 6 位");
    console_set_color(COL_BODY);
    for (i = 0; i < 3; i++) {
        char *v = i == 0 ? s_pwd.old_pwd : i == 1 ? s_pwd.new_pwd : s_pwd.confirm;
        int row = L_fy + i, j;
        size_t n = strlen(v);
        console_set_color(s_pwd.focus == i ? COL_ACCENT : COL_BODY);
        console_write_at(px + 4, row, labels[i]);
        console_set_color(COL_BODY);
        for (j = 0; j < L_fw; j++) buf[j] = j < (int)n ? '*' : ' ';
        buf[L_fw] = '\0';
        if (s_pwd.focus != i) console_set_color(COL_HINT);
        console_write_at(L_fx, row, buf);
        console_set_color(COL_BODY);
    }
    ui_sep(px + 1, py + 6, pw - 2);
    widget_set_rect(g_pwd_ok, px + 7, py + 7, 14, 1);
    widget_set_rect(g_pwd_cancel, px + pw - 21, py + 7, 14, 1);
    widget_draw(g_pwd_ok);
    widget_draw(g_pwd_cancel);
    draw_msg(px + 3, L_msg_y);
}

static bool handle_change_pwd(const InputEvent *ev) {
    static char *fields[3];
    fields[0] = s_pwd.old_pwd;
    fields[1] = s_pwd.new_pwd;
    fields[2] = s_pwd.confirm;
    if (ev->type == EV_KEY && ev->key_down) {
        if (ev->vk == VK_TAB) { s_pwd.focus = (s_pwd.focus + 1) % 3; return true; }
        if (ev->vk == VK_RETURN) {
            if (s_pwd.focus == 2) pwd_save();
            else s_pwd.focus++;
            return true;
        }
        if (ev->vk == VK_BACK) { utf8_backspace(fields[s_pwd.focus]); return true; }
        if (ev->ch >= 0x20) { field_append(fields[s_pwd.focus], 64, ev->ch); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE) {
        bool changed = hover_changed(ev, (Widget *[]){g_pwd_ok, g_pwd_cancel}, 2);
        if (left_press_edge(ev)) {
            if (ev->y >= L_fy && ev->y < L_fy + 3 &&
                ev->x >= L_fx - 4 && ev->x < L_fx + L_fw) {
                s_pwd.focus = ev->y - L_fy;
                return true;
            }
            if (widget_hit(g_pwd_ok, ev->x, ev->y)) { pwd_save(); return true; }
            if (widget_hit(g_pwd_cancel, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* =====================================================================
 * M3：读者管理 / 借阅管理 / 预约管理 / 罚款管理
 * ===================================================================== */

static const char *ui_reader_status(ReaderStatus s) {
    switch (s) {
    case READER_FROZEN: return "冻结";
    case READER_DELETED: return "注销";
    default: return "正常";
    }
}

static const char *ui_loan_status(LoanStatus s) {
    switch (s) {
    case LOAN_RETURNED: return "已还";
    case LOAN_OVERDUE: return "逾期";
    case LOAN_RENEWED: return "续借";
    default: return "借出";
    }
}

static const char *ui_res_status(ReservationStatus s) {
    switch (s) {
    case RES_AVAILABLE: return "可借";
    case RES_PICKED: return "已取";
    case RES_EXPIRED: return "过期";
    case RES_CANCELLED: return "取消";
    default: return "排队";
    }
}

static const char *ui_fine_status(FineStatus s) {
    return s == FINE_PAID ? "已缴" : "未缴";
}

static const char *book_title_of(EntityId id) {
    const Book *b = repo_find_book(id);
    return b ? b->title : "(图书已删除)";
}

static const char *reader_name_of(EntityId id) {
    const Reader *r = repo_find_reader(id);
    return r ? r->name : "(读者已注销)";
}

/* ---------- 表格 ---------- */
typedef struct {
    const char *title;
    int ncols;
    const char *headers[8];
    int widths[8];
} TableDef;

static const TableDef TBL_READER = {
    "读者列表", 6, {"ID", "学号", "姓名", "手机号", "状态", "欠款"},
    {4, 14, 12, 14, 6, 8}
};
static const TableDef TBL_LOAN = {
    "借阅记录", 6, {"单号", "书名", "副本", "读者", "应还日期", "状态"},
    {5, 24, 5, 12, 11, 6}
};
static const TableDef TBL_RES = {
    "预约队列", 5, {"单号", "书名", "读者", "预约日期", "状态"},
    {5, 24, 12, 11, 8}
};
static const TableDef TBL_FINE = {
    "罚款单", 5, {"单号", "读者", "金额", "生成时间", "状态"},
    {5, 12, 9, 20, 6}
};

static void table_header_line(char *out, size_t cap, const TableDef *t) {
    int i;
    out[0] = '\0';
    for (i = 0; i < t->ncols; i++) ui_append(out, cap, t->headers[i], t->widths[i]);
}

static void table_data_line(char *out, size_t cap, const TableDef *t,
                            const char *const *cells) {
    int i;
    out[0] = '\0';
    for (i = 0; i < t->ncols; i++)
        ui_append(out, cap, cells[i] ? cells[i] : "", t->widths[i]);
}

static void table_geom(int *panel_y, int *panel_h) {
    *panel_y = 3;
    *panel_h = s_rows - *panel_y - 4;
    if (*panel_h < 6) *panel_h = 6;
}

static void table_screen(const TableDef *t, int panel_y, int panel_h, Widget *lw) {
    char hdr[200];
    ui_panel(2, panel_y, s_cols - 4, panel_h, NULL);
    console_set_color(COL_TITLE);
    console_write_at(4, panel_y, t->title);
    console_set_color(COL_BODY);
    table_header_line(hdr, sizeof hdr, t);
    console_set_color(COL_ACCENT);
    console_write_at(4, panel_y + 1, hdr);
    console_set_color(COL_BODY);
    ui_sep(3, panel_y + 2, s_cols - 6);
    widget_set_rect(lw, 4, panel_y + 3, s_cols - 8, panel_h - 4);
    widget_draw(lw);
}

/* ================= 读者管理（FR-401~404） ================= */

static void rl_run_search(void) {
    s_rl_count = 0;
    search_readers(s_rl_keyword, s_rl_res, M3_MAX, &s_rl_count);
    s_rl_rebuild = true;
}

static void rl_rebuild_rows(void) {
    char **rows;
    size_t i, made = 0;
    if (!s_rl_rebuild) return;
    s_rl_rebuild = false;
    if (s_rl_count == 0) {
        widget_list_set_items(g_reader_lw, NULL, 0);
        return;
    }
    rows = malloc(s_rl_count * sizeof(char *));
    if (!rows) {
        widget_list_set_items(g_reader_lw, NULL, 0);
        return;
    }
    for (i = 0; i < s_rl_count; i++) {
        const Reader *r = s_rl_res[i];
        char idb[16], debt[24];
        const char *cells[6];
        char *line = malloc(160);
        if (!line) break;
        snprintf(idb, sizeof idb, "%d", r->id);
        snprintf(debt, sizeof debt, "%.2f", reader_total_debt(r->id));
        cells[0] = idb;
        cells[1] = r->reader_no;
        cells[2] = r->name;
        cells[3] = r->phone;
        cells[4] = ui_reader_status(r->status);
        cells[5] = debt;
        table_data_line(line, 160, &TBL_READER, cells);
        rows[made++] = line;
    }
    widget_list_set_items(g_reader_lw, (const char *const *)rows, made);
    for (i = 0; i < made; i++) free(rows[i]);
    free(rows);
    widget_list_set_sel(g_reader_lw, 0);
}

static const Reader *rl_selected(void) {
    int sel = widget_list_sel(g_reader_lw);
    if (sel < 0 || (size_t)sel >= s_rl_count) return NULL;
    return s_rl_res[sel];
}

static void rl_open_form(EntityId id) {
    const Reader *r;
    memset(s_rl_name, 0, sizeof s_rl_name);
    memset(s_rl_no, 0, sizeof s_rl_no);
    memset(s_rl_phone, 0, sizeof s_rl_phone);
    memset(s_rl_email, 0, sizeof s_rl_email);
    memset(s_rl_pwd, 0, sizeof s_rl_pwd);
    s_rl_edit_id = id;
    if (id) {
        r = repo_find_reader(id);
        if (r) {
            str_copy(s_rl_name, sizeof s_rl_name, r->name);
            str_copy(s_rl_no, sizeof s_rl_no, r->reader_no);
            str_copy(s_rl_phone, sizeof s_rl_phone, r->phone);
            str_copy(s_rl_email, sizeof s_rl_email, r->email);
        }
    }
    s_rl_focus = 0;
    s_rl_mode = 1;
    s_msg[0] = '\0';
}

static void rl_save(void) {
    Result r;
    if (str_empty(s_rl_name) || str_empty(s_rl_no)) {
        str_copy(s_msg, sizeof s_msg, "姓名与学号不能为空");
        return;
    }
    if (s_rl_edit_id == 0) { /* 新增：交给 auth 做散列与校验 */
        Reader rd;
        memset(&rd, 0, sizeof rd);
        str_copy(rd.name, sizeof rd.name, s_rl_name);
        str_copy(rd.reader_no, sizeof rd.reader_no, s_rl_no);
        str_copy(rd.phone, sizeof rd.phone, s_rl_phone);
        str_copy(rd.email, sizeof rd.email, s_rl_email);
        r = auth_register_reader(&rd, s_rl_pwd);
    } else {
        Reader rd;
        const Reader *cur = repo_find_reader(s_rl_edit_id);
        if (!cur) {
            str_copy(s_msg, sizeof s_msg, "读者不存在");
            return;
        }
        rd = *cur;
        str_copy(rd.name, sizeof rd.name, s_rl_name);
        str_copy(rd.reader_no, sizeof rd.reader_no, s_rl_no);
        str_copy(rd.phone, sizeof rd.phone, s_rl_phone);
        str_copy(rd.email, sizeof rd.email, s_rl_email);
        if (!str_empty(s_rl_pwd)) { /* 重置密码 */
            hash_random_salt(rd.salt, sizeof rd.salt);
            hash_password(s_rl_pwd, rd.salt, rd.password_hash, sizeof rd.password_hash);
        }
        r = reader_update(&rd);
    }
    if (r == RES_OK) {
        str_copy(s_msg, sizeof s_msg, s_rl_edit_id ? "读者资料已保存" : "读者已新增");
        s_rl_mode = 0;
        rl_run_search();
    } else if (r == RES_ERR_DUP) {
        str_copy(s_msg, sizeof s_msg, "该学号/工号已存在");
    } else if (r == RES_ERR_INVALID) {
        str_copy(s_msg, sizeof s_msg, "保存失败：请检查输入（密码≥6位、手机/邮箱格式）");
    } else {
        snprintf(s_msg, sizeof s_msg, "保存失败 (code=%d)", r);
    }
}

static void rl_toggle_freeze(void) {
    const Reader *r = rl_selected();
    Result rc;
    if (!r) {
        str_copy(s_msg, sizeof s_msg, "请先选中一位读者");
        return;
    }
    rc = (r->status == READER_FROZEN) ? reader_unfreeze(r->id) : reader_freeze(r->id);
    if (rc == RES_OK) {
        snprintf(s_msg, sizeof s_msg, "%s（%s）",
                 r->status == READER_FROZEN ? "已解冻" : "已冻结", r->name);
    } else {
        snprintf(s_msg, sizeof s_msg, "操作失败 (code=%d)", rc);
    }
    rl_run_search();
}

static void rl_delete(void) {
    const Reader *r = rl_selected();
    Result rc;
    if (!r) {
        str_copy(s_msg, sizeof s_msg, "请先选中一位读者");
        return;
    }
    if (s_pending_del == r->id && s_pending_kind == 4) {
        s_pending_del = 0;
        rc = reader_delete(r->id);
        if (rc == RES_OK) str_copy(s_msg, sizeof s_msg, "读者已注销");
        else if (rc == RES_ERR) str_copy(s_msg, sizeof s_msg, "无法注销：该读者有未还图书");
        else snprintf(s_msg, sizeof s_msg, "注销失败 (code=%d)", rc);
        rl_run_search();
    } else {
        s_pending_del = r->id;
        s_pending_kind = 4;
        str_copy(s_msg, sizeof s_msg, "再按一次 D/注销 确认注销该读者");
    }
}

static void rl_show_detail(void) {
    const Reader *r = rl_selected();
    int cnt = 0;
    if (!r) {
        str_copy(s_msg, sizeof s_msg, "请先选中一位读者");
        return;
    }
    reader_borrow_count(r->id, &cnt);
    snprintf(s_msg, sizeof s_msg, "%.20s（%.20s）· 状态 %s · 在借 %d 本 · 欠款 %.2f 元",
             r->name, r->reader_no, ui_reader_status(r->status), cnt,
             reader_total_debt(r->id));
}

static void draw_reader_list(void) {
    char buf[160];
    int panel_y, panel_h, by, i;

    if (s_rl_mode == 1) { /* ---- 表单 ---- */
        static const char *labels[] = {"姓名", "学号/工号", "手机号", "邮箱", "密码"};
        int pw = 54, ph = 13, px, py = 3;
        if (pw > s_cols - 8) pw = s_cols - 8;
        px = (s_cols - pw) / 2;
        L_px = px;
        L_py = py;
        L_pw = pw;
        L_ph = ph;
        L_fx = px + 14;
        L_fw = pw - 18;
        L_fy = py + 3;
        L_msg_y = py + ph - 2;
        ui_panel(px, py, pw, ph, s_rl_edit_id ? "编辑读者" : "新增读者");
        console_set_color(COL_HINT);
        console_write_at(px + 3, py + 1,
                         s_rl_edit_id ? "密码留空表示不修改" : "初始密码不少于 6 位");
        console_set_color(COL_BODY);
        for (i = 0; i < 5; i++) {
            char *v = i == 0 ? s_rl_name : i == 1 ? s_rl_no : i == 2 ? s_rl_phone
                      : i == 3 ? s_rl_email : s_rl_pwd;
            int row = L_fy + i, j;
            size_t n = strlen(v);
            console_set_color(s_rl_focus == i ? COL_ACCENT : COL_BODY);
            console_write_at(px + 4, row, labels[i]);
            console_set_color(COL_BODY);
            for (j = 0; j < L_fw; j++) {
                if (j < (int)n) buf[j] = (i == 4) ? '*' : v[j];
                else buf[j] = ' ';
            }
            buf[L_fw] = '\0';
            if (s_rl_focus != i) console_set_color(COL_HINT);
            console_write_at(L_fx, row, buf);
            console_set_color(COL_BODY);
        }
        ui_sep(px + 1, py + 9, pw - 2);
        widget_set_rect(g_rl_save, px + 18, py + 10, 14, 1);
        widget_set_rect(g_rl_cancel, px + pw - 32, py + 10, 14, 1);
        widget_draw(g_rl_save);
        widget_draw(g_rl_cancel);
        draw_msg(px + 3, L_msg_y);
        draw_hint_bar("Tab 切换字段 · Enter 下一项/保存 · Esc 取消");
        return;
    }

    /* ---- 列表 ---- */
    rl_rebuild_rows();
    {
        int bar_w = 60, sx;
        if (bar_w > s_cols - 4) bar_w = s_cols - 4;
        sx = (s_cols - bar_w) / 2;
        if (sx < 1) sx = 1;
        L_sx = sx;
        console_set_color(COL_HINT);
        console_write_at(sx, 1, "检索 学号 / 姓名 / 手机号");
        console_set_color(COL_BODY);
        snprintf(buf, sizeof buf, "[%-20s]", s_rl_keyword);
        if (s_rl_typing) console_set_color(ui_invert(COL_BODY));
        console_write_at(sx + 28, 1, buf);
        console_set_color(COL_BODY);
    }
    table_geom(&panel_y, &panel_h);
    table_screen(&TBL_READER, panel_y, panel_h, g_reader_lw);
    draw_msg(4, s_rows - 4);
    by = s_rows - 3;
    widget_set_rect(g_rl_new, 4, by, 12, 1);
    widget_set_rect(g_rl_edit, 18, by, 12, 1);
    widget_set_rect(g_rl_freeze, 32, by, 16, 1);
    widget_set_rect(g_rl_del, 50, by, 12, 1);
    widget_set_rect(g_rl_view, 64, by, 12, 1);
    widget_set_rect(g_rl_back, 78, by, 12, 1);
    widget_draw(g_rl_new);
    widget_draw(g_rl_edit);
    widget_draw(g_rl_freeze);
    widget_draw(g_rl_del);
    widget_draw(g_rl_view);
    widget_draw(g_rl_back);
    draw_hint_bar("↑↓ 选择 · A 新增 · E 编辑 · F 冻结/解冻 · D 注销 · V 详情");
}

static bool handle_reader_list(const InputEvent *ev) {
    if (s_rl_mode == 1) { /* 表单 */
        static char *fields[5];
        int n = 5;
        fields[0] = s_rl_name;
        fields[1] = s_rl_no;
        fields[2] = s_rl_phone;
        fields[3] = s_rl_email;
        fields[4] = s_rl_pwd;
        if (ev->type == EV_KEY && ev->key_down) {
            if (ev->vk == VK_TAB) { s_rl_focus = (s_rl_focus + 1) % n; return true; }
            if (ev->vk == VK_RETURN) {
                if (s_rl_focus == n - 1) rl_save();
                else s_rl_focus++;
                return true;
            }
            if (ev->vk == VK_BACK) { utf8_backspace(fields[s_rl_focus]); return true; }
            if (ev->ch >= 0x20) {
                size_t cap = (s_rl_focus == 1) ? sizeof s_rl_no : 64;
                field_append(fields[s_rl_focus], cap, ev->ch);
                return true;
            }
            return false;
        }
        if (ev->type == EV_MOUSE) {
            bool changed = hover_changed(ev, (Widget *[]){g_rl_save, g_rl_cancel}, 2);
            if (left_press_edge(ev)) {
                if (ev->y >= L_fy && ev->y < L_fy + n &&
                    ev->x >= L_fx - 2 && ev->x < L_fx + L_fw) {
                    s_rl_focus = ev->y - L_fy;
                    return true;
                }
                if (widget_hit(g_rl_save, ev->x, ev->y)) { rl_save(); return true; }
                if (widget_hit(g_rl_cancel, ev->x, ev->y)) {
                    s_rl_mode = 0;
                    s_msg[0] = '\0';
                    return true;
                }
            }
            return changed;
        }
        return false;
    }

    /* 列表模式 */
    if (ev->type == EV_KEY && ev->key_down) {
        int sel = widget_list_sel(g_reader_lw);
        if (s_rl_typing) {
            if (ev->vk == VK_RETURN) { s_rl_typing = false; rl_run_search(); return true; }
            if (ev->vk == VK_BACK) { utf8_backspace(s_rl_keyword); return true; }
            if (ev->ch >= 0x20) { field_append(s_rl_keyword, sizeof s_rl_keyword, ev->ch); return true; }
            return false;
        }
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_reader_lw, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_reader_lw, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(g_reader_lw, 0); return true;
        case VK_END: widget_list_set_sel(g_reader_lw, (int)s_rl_count - 1); return true;
        case VK_RETURN: rl_show_detail(); return true;
        default: break;
        }
        if (ev->ch == 'a' || ev->ch == 'A') { rl_open_form(0); return true; }
        if (ev->ch == 'e' || ev->ch == 'E') {
            const Reader *r = rl_selected();
            if (r) rl_open_form(r->id);
            return true;
        }
        if (ev->ch == 'f' || ev->ch == 'F') { rl_toggle_freeze(); return true; }
        if (ev->ch == 'd' || ev->ch == 'D') { rl_delete(); return true; }
        if (ev->ch == 'v' || ev->ch == 'V') { rl_show_detail(); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_reader_lw, widget_list_sel(g_reader_lw) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        Widget *btns[6] = {g_rl_new, g_rl_edit, g_rl_freeze, g_rl_del, g_rl_view, g_rl_back};
        bool changed = hover_changed(ev, btns, 6);
        if (left_press_edge(ev)) {
            if (ev->y == 1 && ev->x >= L_sx + 28 && ev->x < L_sx + 50) {
                s_rl_typing = true;
                return true;
            }
            widget_get_rect(g_reader_lw, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                int row = ev->y - ly;
                if (row < (int)s_rl_count) widget_list_set_sel(g_reader_lw, row);
                return true;
            }
            if (widget_hit(g_rl_new, ev->x, ev->y)) rl_open_form(0);
            else if (widget_hit(g_rl_edit, ev->x, ev->y)) {
                const Reader *r = rl_selected();
                if (r) rl_open_form(r->id);
            } else if (widget_hit(g_rl_freeze, ev->x, ev->y)) rl_toggle_freeze();
            else if (widget_hit(g_rl_del, ev->x, ev->y)) rl_delete();
            else if (widget_hit(g_rl_view, ev->x, ev->y)) rl_show_detail();
            else if (widget_hit(g_rl_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* ================= 借阅管理（FR-501~506） ================= */

static void loan_refresh(void) {
    const Loan *pool[M3_MAX];
    size_t n = 0, i, m = 0;
    if (s_loan_tab == 2) {
        loan_list_overdue(pool, M3_MAX, &n);
    } else if (s_loan_mine) {
        loan_list_by_reader(g_session.user_id, pool, M3_MAX, &n);
    } else {
        const Loan *loans;
        size_t total = 0;
        loans = repo_loans(&total);
        for (i = 0; i < total && n < M3_MAX; i++)
            if (loans[i].status == LOAN_ACTIVE || loans[i].status == LOAN_OVERDUE)
                pool[n++] = &loans[i];
    }
    for (i = 0; i < n && m < M3_MAX; i++) {
        if (s_loan_mine && pool[i]->reader_id != g_session.user_id) continue;
        s_loan_res[m++] = pool[i];
    }
    s_loan_count = m;
    s_loan_rebuild = true;
}

static void loan_rebuild_rows(void) {
    char **rows;
    size_t i, made = 0;
    if (!s_loan_rebuild) return;
    s_loan_rebuild = false;
    if (s_loan_count == 0) {
        widget_list_set_items(g_loan_lw, NULL, 0);
        return;
    }
    rows = malloc(s_loan_count * sizeof(char *));
    if (!rows) {
        widget_list_set_items(g_loan_lw, NULL, 0);
        return;
    }
    for (i = 0; i < s_loan_count; i++) {
        const Loan *l = s_loan_res[i];
        char idb[16], copy[8];
        const char *cells[6];
        char *line = malloc(160);
        if (!line) break;
        snprintf(idb, sizeof idb, "%d", l->id);
        snprintf(copy, sizeof copy, "%d", l->copy_no);
        cells[0] = idb;
        cells[1] = book_title_of(l->book_id);
        cells[2] = copy;
        cells[3] = reader_name_of(l->reader_id);
        cells[4] = l->due_date;
        cells[5] = ui_loan_status(l->status);
        table_data_line(line, 160, &TBL_LOAN, cells);
        rows[made++] = line;
    }
    widget_list_set_items(g_loan_lw, (const char *const *)rows, made);
    for (i = 0; i < made; i++) free(rows[i]);
    free(rows);
    widget_list_set_sel(g_loan_lw, 0);
}

static const Loan *loan_selected(void) {
    int sel = widget_list_sel(g_loan_lw);
    if (sel < 0 || (size_t)sel >= s_loan_count) return NULL;
    return s_loan_res[sel];
}

static void loan_do_borrow(void) {
    const Reader *rd;
    const Book *bk;
    Loan *out = NULL;
    Result r;
    if (reader_find_by_no(s_loan_no, &rd) != RES_OK) {
        str_copy(s_msg, sizeof s_msg, "借出失败：读者学号不存在");
        return;
    }
    if (book_find_by_isbn(s_loan_isbn, &bk) != RES_OK) {
        str_copy(s_msg, sizeof s_msg, "借出失败：图书 ISBN 不存在");
        return;
    }
    r = loan_borrow(rd->id, bk->id, 0, &out);
    if (r == RES_OK && out) {
        snprintf(s_msg, sizeof s_msg, "借出成功：%.16s · %.30s · 应还 %.11s",
                 rd->name, bk->title, out->due_date);
        s_loan_no[0] = '\0';
        s_loan_isbn[0] = '\0';
        loan_refresh();
    } else if (r == RES_ERR_FROZEN) {
        str_copy(s_msg, sizeof s_msg, "借出失败：读者已冻结或欠款超过阈值");
    } else if (r == RES_ERR_LIMIT) {
        str_copy(s_msg, sizeof s_msg, "借出失败：该读者已达借阅上限");
    } else if (r == RES_ERR_NOT_FOUND) {
        str_copy(s_msg, sizeof s_msg, "借出失败：读者或图书不存在");
    } else {
        str_copy(s_msg, sizeof s_msg, "借出失败：图书无可借副本或被停借");
    }
}

static void loan_do_return(void) {
    const Loan *l = loan_selected();
    Result r;
    if (!l) {
        str_copy(s_msg, sizeof s_msg, "请先选中一条借阅记录");
        return;
    }
    r = loan_return_by_loan(l->id);
    if (r == RES_OK) snprintf(s_msg, sizeof s_msg, "已归还：%.40s（单号 %d）", book_title_of(l->book_id), l->id);
    else snprintf(s_msg, sizeof s_msg, "归还失败 (code=%d)", r);
    loan_refresh();
}

static void loan_do_renew(void) {
    const Loan *l = loan_selected();
    Result r;
    if (!l) {
        str_copy(s_msg, sizeof s_msg, "请先选中一条借阅记录");
        return;
    }
    r = loan_renew(l->id);
    if (r == RES_OK) snprintf(s_msg, sizeof s_msg, "续借成功：应还日期已延长 15 天");
    else if (r == RES_ERR_LIMIT) str_copy(s_msg, sizeof s_msg, "续借失败：已续借过一次");
    else if (r == RES_ERR) str_copy(s_msg, sizeof s_msg, "续借失败：已逾期或该书有预约排队");
    else snprintf(s_msg, sizeof s_msg, "续借失败 (code=%d)", r);
    loan_refresh();
}

static void draw_loan_manage(void) {
    int panel_y, panel_h, by;
    char label0[16], label1[16];

    if (s_loan_mine) {
        snprintf(label0, sizeof label0, "1 我的借阅");
        snprintf(label1, sizeof label1, "2 我的逾期");
    } else {
        snprintf(label0, sizeof label0, "1 借出");
        snprintf(label1, sizeof label1, "2 在借");
    }
    widget_set_rect(g_ln_tab0, 4, 1, 14, 1);
    widget_set_rect(g_ln_tab1, 20, 1, 14, 1);
    widget_set_rect(g_ln_tab2, 36, 1, 14, 1);
    widget_set_selected(g_ln_tab0, s_loan_tab == 0);
    widget_set_selected(g_ln_tab1, s_loan_tab == 1);
    widget_set_selected(g_ln_tab2, s_loan_tab == 2);
    /* 页签文字随角色变化：用 set_rect + 重新绘制标签 */
    {
        char buf[24];
        console_set_color(s_loan_tab == 0 ? COL_ACCENT : COL_BODY);
        snprintf(buf, sizeof buf, "[%s]", label0);
        console_write_at(4, 1, buf);
        console_set_color(s_loan_tab == 1 ? COL_ACCENT : COL_BODY);
        snprintf(buf, sizeof buf, "[%s]", label1);
        console_write_at(20, 1, buf);
        if (!s_loan_mine) {
            console_set_color(s_loan_tab == 2 ? COL_ACCENT : COL_BODY);
            console_write_at(36, 1, "[3 逾期]");
        }
        console_set_color(COL_BODY);
    }

    if (s_loan_tab == 0 && !s_loan_mine) { /* ---- 借出表单 ---- */
        char buf[80];
        int pw = 56, ph = 11, px, py = 3, i;
        if (pw > s_cols - 8) pw = s_cols - 8;
        px = (s_cols - pw) / 2;
        L_px = px;
        L_py = py;
        L_pw = pw;
        L_ph = ph;
        L_fx = px + 16;
        L_fw = pw - 20;
        L_fy = py + 3;
        L_msg_y = py + ph - 2;
        ui_panel(px, py, pw, ph, "办理借书");
        console_set_color(COL_HINT);
        console_write_at(px + 3, py + 1, "输入读者学号与图书 ISBN，Enter 切换 / 借出");
        console_set_color(COL_BODY);
        for (i = 0; i < 2; i++) {
            const char *v = i == 0 ? s_loan_no : s_loan_isbn;
            int row = L_fy + i;
            console_set_color(s_loan_focus == i ? COL_ACCENT : COL_BODY);
            console_write_at(px + 4, row, i == 0 ? "读者学号" : "图书 ISBN");
            console_set_color(COL_BODY);
            ui_pad(buf, sizeof buf, v, L_fw);
            if (s_loan_focus != i) console_set_color(COL_HINT);
            console_write_at(L_fx, row, buf);
            console_set_color(COL_BODY);
        }
        ui_sep(px + 1, py + 6, pw - 2);
        widget_set_rect(g_ln_borrow, px + 20, py + 7, 16, 1);
        widget_draw(g_ln_borrow);
        draw_msg(px + 3, L_msg_y);
        draw_hint_bar("Tab 切换字段 · Enter 借出 · Esc 返回");
        return;
    }

    /* ---- 借阅列表 ---- */
    loan_rebuild_rows();
    table_geom(&panel_y, &panel_h);
    {
        TableDef t = TBL_LOAN;
        if (s_loan_mine) {
            t.title = "我的借阅";
        } else if (s_loan_tab == 2) {
            t.title = "逾期未还";
        }
        table_screen(&t, panel_y, panel_h, g_loan_lw);
    }
    draw_msg(4, s_rows - 4);
    by = s_rows - 3;
    if (!s_loan_mine && s_loan_tab != 2) {
        widget_set_rect(g_ln_borrow, 4, by, 16, 1);
        console_set_color(COL_ACCENT);
        console_write_at(4, by, "[Enter 归还]");
        console_set_color(COL_BODY);
    }
    console_set_color(COL_ACCENT);
    console_write_at(24, by, "[R 续借]");
    console_set_color(COL_BODY);
    widget_set_rect(g_ln_back, 78, by, 12, 1);
    widget_draw(g_ln_back);
    draw_hint_bar(s_loan_mine ? "↑↓ 选择 · R 续借 · Esc 返回"
                              : "↑↓ 选择 · Enter 归还 · R 续借 · 1/2/3 切换页签");
}

static bool handle_loan_manage(const InputEvent *ev) {
    if (s_loan_tab == 0 && !s_loan_mine) { /* 借出表单 */
        static char *fields[2];
        fields[0] = s_loan_no;
        fields[1] = s_loan_isbn;
        if (ev->type == EV_KEY && ev->key_down) {
            if (ev->vk == VK_TAB) { s_loan_focus = (s_loan_focus + 1) % 2; return true; }
            if (ev->vk == VK_RETURN) {
                if (s_loan_focus == 0) s_loan_focus = 1;
                else loan_do_borrow();
                return true;
            }
            if (ev->vk == VK_BACK) { utf8_backspace(fields[s_loan_focus]); return true; }
            if (ev->ch >= 0x20) { field_append(fields[s_loan_focus], 32, ev->ch); return true; }
            return false;
        }
        if (ev->type == EV_MOUSE) {
            bool changed = hover_changed(ev, (Widget *[]){g_ln_borrow}, 1);
            if (left_press_edge(ev)) {
                if (ev->y >= L_fy && ev->y < L_fy + 2 &&
                    ev->x >= L_fx - 2 && ev->x < L_fx + L_fw) {
                    s_loan_focus = ev->y - L_fy;
                    return true;
                }
                if (widget_hit(g_ln_borrow, ev->x, ev->y)) { loan_do_borrow(); return true; }
                if (widget_hit(g_ln_tab1, ev->x, ev->y)) { s_loan_tab = 1; loan_refresh(); return true; }
                if (!s_loan_mine && widget_hit(g_ln_tab2, ev->x, ev->y)) { s_loan_tab = 2; loan_refresh(); return true; }
            }
            return changed;
        }
        return false;
    }

    /* 列表模式 */
    if (ev->type == EV_KEY && ev->key_down) {
        int sel = widget_list_sel(g_loan_lw);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_loan_lw, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_loan_lw, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(g_loan_lw, 0); return true;
        case VK_END: widget_list_set_sel(g_loan_lw, (int)s_loan_count - 1); return true;
        case VK_RETURN:
            if (!s_loan_mine && s_loan_tab != 2) loan_do_return();
            return true;
        default: break;
        }
        if (ev->ch >= '1' && ev->ch <= '3') {
            int tab = ev->ch - '1';
            if (s_loan_mine) {
                if (tab == 0) tab = 1;      /* 我的借阅 */
                else if (tab == 1) tab = 2; /* 我的逾期 */
                else return true;
            }
            s_loan_tab = tab;
            loan_refresh();
            return true;
        }
        if (ev->ch == 'r' || ev->ch == 'R') { loan_do_renew(); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_loan_lw, widget_list_sel(g_loan_lw) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        bool changed = hover_changed(ev, (Widget *[]){g_ln_back}, 1);
        if (left_press_edge(ev)) {
            if (widget_hit(g_ln_tab0, ev->x, ev->y)) { s_loan_tab = s_loan_mine ? 1 : 0; loan_refresh(); return true; }
            if (widget_hit(g_ln_tab1, ev->x, ev->y)) { s_loan_tab = s_loan_mine ? 2 : 1; loan_refresh(); return true; }
            if (!s_loan_mine && widget_hit(g_ln_tab2, ev->x, ev->y)) { s_loan_tab = 2; loan_refresh(); return true; }
            widget_get_rect(g_loan_lw, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                int row = ev->y - ly;
                if (row < (int)s_loan_count) widget_list_set_sel(g_loan_lw, row);
                return true;
            }
            if (widget_hit(g_ln_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* ================= 预约管理（FR-601~604） ================= */

static void res_refresh(void) {
    size_t i, m = 0;
    if (s_res_mine) {
        reservation_list_by_reader(g_session.user_id, s_res_res, M3_MAX, &s_res_count);
    } else {
        const Reservation *all;
        size_t n = 0;
        all = repo_reservations(&n);
        for (i = 0; i < n && m < M3_MAX; i++) s_res_res[m++] = &all[i];
        s_res_count = m;
    }
    s_res_rebuild = true;
}

static void res_rebuild_rows(void) {
    char **rows;
    size_t i, made = 0;
    if (!s_res_rebuild) return;
    s_res_rebuild = false;
    if (s_res_count == 0) {
        widget_list_set_items(g_res_lw, NULL, 0);
        return;
    }
    rows = malloc(s_res_count * sizeof(char *));
    if (!rows) {
        widget_list_set_items(g_res_lw, NULL, 0);
        return;
    }
    for (i = 0; i < s_res_count; i++) {
        const Reservation *r = s_res_res[i];
        char idb[16];
        const char *cells[5];
        char *line = malloc(160);
        if (!line) break;
        snprintf(idb, sizeof idb, "%d", r->id);
        cells[0] = idb;
        cells[1] = book_title_of(r->book_id);
        cells[2] = reader_name_of(r->reader_id);
        cells[3] = r->reserve_date;
        cells[4] = ui_res_status(r->status);
        table_data_line(line, 160, &TBL_RES, cells);
        rows[made++] = line;
    }
    widget_list_set_items(g_res_lw, (const char *const *)rows, made);
    for (i = 0; i < made; i++) free(rows[i]);
    free(rows);
    widget_list_set_sel(g_res_lw, 0);
}

static const Reservation *res_selected(void) {
    int sel = widget_list_sel(g_res_lw);
    if (sel < 0 || (size_t)sel >= s_res_count) return NULL;
    return s_res_res[sel];
}

static void res_do_cancel(void) {
    const Reservation *r = res_selected();
    Result rc;
    if (!r) {
        str_copy(s_msg, sizeof s_msg, "请先选中一条预约");
        return;
    }
    rc = reservation_cancel(r->id);
    if (rc == RES_OK) snprintf(s_msg, sizeof s_msg, "预约已取消（单号 %d）", r->id);
    else snprintf(s_msg, sizeof s_msg, "取消失败 (code=%d)", rc);
    res_refresh();
}

static void res_do_process(void) {
    const Reservation *r = res_selected();
    Result rc;
    if (!r) {
        str_copy(s_msg, sizeof s_msg, "请先选中一条预约");
        return;
    }
    rc = reservation_process_next(r->book_id);
    if (rc == RES_OK) snprintf(s_msg, sizeof s_msg, "已通知下一位预约者：%.40s", book_title_of(r->book_id));
    else str_copy(s_msg, sizeof s_msg, "没有排队中的预约者");
    res_refresh();
}

static void draw_reservation_manage(void) {
    int panel_y, panel_h, by;
    TableDef t = TBL_RES;
    res_rebuild_rows();
    table_geom(&panel_y, &panel_h);
    if (s_res_mine) t.title = "我的预约";
    table_screen(&t, panel_y, panel_h, g_res_lw);
    draw_msg(4, s_rows - 4);
    by = s_rows - 3;
    widget_set_rect(g_rs_cancel, 4, by, 14, 1);
    widget_set_rect(g_rs_proc, 22, by, 18, 1);
    widget_set_rect(g_rs_back, 78, by, 12, 1);
    widget_draw(g_rs_cancel);
    if (!s_res_mine) widget_draw(g_rs_proc);
    widget_draw(g_rs_back);
    draw_hint_bar(s_res_mine ? "↑↓ 选择 · C 取消预约 · Esc 返回"
                             : "↑↓ 选择 · C 取消预约 · P 处理下一位 · Esc 返回");
}

static bool handle_reservation_manage(const InputEvent *ev) {
    if (ev->type == EV_KEY && ev->key_down) {
        int sel = widget_list_sel(g_res_lw);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_res_lw, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_res_lw, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(g_res_lw, 0); return true;
        case VK_END: widget_list_set_sel(g_res_lw, (int)s_res_count - 1); return true;
        default: break;
        }
        if (ev->ch == 'c' || ev->ch == 'C') { res_do_cancel(); return true; }
        if (!s_res_mine && (ev->ch == 'p' || ev->ch == 'P')) { res_do_process(); return true; }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_res_lw, widget_list_sel(g_res_lw) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        Widget *btns[3] = {g_rs_cancel, g_rs_proc, g_rs_back};
        bool changed = hover_changed(ev, btns, 3);
        if (left_press_edge(ev)) {
            widget_get_rect(g_res_lw, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                int row = ev->y - ly;
                if (row < (int)s_res_count) widget_list_set_sel(g_res_lw, row);
                return true;
            }
            if (widget_hit(g_rs_cancel, ev->x, ev->y)) res_do_cancel();
            else if (!s_res_mine && widget_hit(g_rs_proc, ev->x, ev->y)) res_do_process();
            else if (widget_hit(g_rs_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

/* ================= 罚款管理（FR-701~704） ================= */

static void fine_refresh(void) {
    size_t i, m = 0;
    if (s_fine_mine) {
        fine_list_by_reader(g_session.user_id, s_fine_res, M3_MAX, &s_fine_count);
    } else if (s_fine_tab == 0) {
        fine_list_unpaid(s_fine_res, M3_MAX, &s_fine_count);
    } else {
        const Fine *all;
        size_t n = 0;
        all = repo_fines(&n);
        for (i = 0; i < n && m < M3_MAX; i++)
            if (all[i].status == FINE_PAID) s_fine_res[m++] = &all[i];
        s_fine_count = m;
    }
    s_fine_rebuild = true;
}

static void fine_rebuild_rows(void) {
    char **rows;
    size_t i, made = 0;
    if (!s_fine_rebuild) return;
    s_fine_rebuild = false;
    if (s_fine_count == 0) {
        widget_list_set_items(g_fine_lw, NULL, 0);
        return;
    }
    rows = malloc(s_fine_count * sizeof(char *));
    if (!rows) {
        widget_list_set_items(g_fine_lw, NULL, 0);
        return;
    }
    for (i = 0; i < s_fine_count; i++) {
        const Fine *f = s_fine_res[i];
        char idb[16], amount[24];
        const char *cells[5];
        char *line = malloc(160);
        if (!line) break;
        snprintf(idb, sizeof idb, "%d", f->id);
        snprintf(amount, sizeof amount, "%.2f", f->amount);
        cells[0] = idb;
        cells[1] = reader_name_of(f->reader_id);
        cells[2] = amount;
        cells[3] = f->created_at;
        cells[4] = ui_fine_status(f->status);
        table_data_line(line, 160, &TBL_FINE, cells);
        rows[made++] = line;
    }
    widget_list_set_items(g_fine_lw, (const char *const *)rows, made);
    for (i = 0; i < made; i++) free(rows[i]);
    free(rows);
    widget_list_set_sel(g_fine_lw, 0);
}

static void fine_do_pay(void) {
    int sel = widget_list_sel(g_fine_lw);
    const Fine *f;
    Result r;
    if (sel < 0 || (size_t)sel >= s_fine_count) {
        str_copy(s_msg, sizeof s_msg, "请先选中一张罚款单");
        return;
    }
    f = s_fine_res[sel];
    r = fine_pay(f->id);
    if (r == RES_OK) snprintf(s_msg, sizeof s_msg, "缴费成功：%.2f 元", f->amount);
    else snprintf(s_msg, sizeof s_msg, "缴费失败 (code=%d)", r);
    fine_refresh();
}

static void draw_fine_manage(void) {
    int panel_y, panel_h, by;
    TableDef t = TBL_FINE;
    double income = fine_total_income();
    char info[64];

    widget_set_rect(g_fi_tab0, 4, 1, 14, 1);
    widget_set_rect(g_fi_tab1, 20, 1, 14, 1);
    console_set_color(s_fine_tab == 0 ? COL_ACCENT : COL_BODY);
    console_write_at(4, 1, "[1 未缴]");
    console_set_color(s_fine_tab == 1 ? COL_ACCENT : COL_BODY);
    console_write_at(20, 1, "[2 已缴]");
    console_set_color(COL_HINT);
    snprintf(info, sizeof info, "罚款总收入 %.2f 元", income);
    console_write_at(40, 1, info);
    console_set_color(COL_BODY);

    fine_rebuild_rows();
    table_geom(&panel_y, &panel_h);
    if (s_fine_mine) t.title = "我的罚款";
    else t.title = s_fine_tab == 0 ? "未缴罚款" : "已缴罚款";
    table_screen(&t, panel_y, panel_h, g_fine_lw);
    draw_msg(4, s_rows - 4);
    by = s_rows - 3;
    if (s_fine_tab == 0 || s_fine_mine) {
        widget_set_rect(g_fi_pay, 4, by, 14, 1);
        console_set_color(COL_ACCENT);
        console_write_at(4, by, s_fine_mine ? "[Enter 缴费]" : "[Enter 收款]");
        console_set_color(COL_BODY);
    }
    widget_set_rect(g_fi_back, 78, by, 12, 1);
    widget_draw(g_fi_back);
    draw_hint_bar(s_fine_mine ? "↑↓ 选择 · Enter 缴费 · Esc 返回"
                              : "↑↓ 选择 · Enter 收款 · 1/2 切换 · Esc 返回");
}

static bool handle_fine_manage(const InputEvent *ev) {
    if (ev->type == EV_KEY && ev->key_down) {
        int sel = widget_list_sel(g_fine_lw);
        switch (ev->vk) {
        case VK_UP: widget_list_set_sel(g_fine_lw, sel - 1); return true;
        case VK_DOWN: widget_list_set_sel(g_fine_lw, sel + 1); return true;
        case VK_HOME: widget_list_set_sel(g_fine_lw, 0); return true;
        case VK_END: widget_list_set_sel(g_fine_lw, (int)s_fine_count - 1); return true;
        case VK_RETURN: fine_do_pay(); return true;
        default: break;
        }
        if (!s_fine_mine && (ev->ch == '1' || ev->ch == '2')) {
            s_fine_tab = ev->ch - '1';
            fine_refresh();
            return true;
        }
        return false;
    }
    if (ev->type == EV_MOUSE_WHEEL) {
        widget_list_set_sel(g_fine_lw, widget_list_sel(g_fine_lw) + (ev->wheel_delta > 0 ? -1 : 1));
        return true;
    }
    if (ev->type == EV_MOUSE) {
        int lx, ly, lw, lh;
        bool changed = hover_changed(ev, (Widget *[]){g_fi_pay, g_fi_back}, 2);
        if (left_press_edge(ev)) {
            if (!s_fine_mine && widget_hit(g_fi_tab0, ev->x, ev->y)) { s_fine_tab = 0; fine_refresh(); return true; }
            if (!s_fine_mine && widget_hit(g_fi_tab1, ev->x, ev->y)) { s_fine_tab = 1; fine_refresh(); return true; }
            widget_get_rect(g_fine_lw, &lx, &ly, &lw, &lh);
            if (ev->y >= ly && ev->y < ly + lh && ev->x >= lx && ev->x < lx + lw) {
                int row = ev->y - ly;
                if (row < (int)s_fine_count) widget_list_set_sel(g_fine_lw, row);
                return true;
            }
            if (widget_hit(g_fi_back, ev->x, ev->y)) {
                s_msg[0] = '\0';
                screen_back();
                return true;
            }
        }
        return changed;
    }
    return false;
}

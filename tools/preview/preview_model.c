#include "preview.h"
#include <string.h>

static void message(Preview *p, const char *s) { text_copy(p->message, TEXT_CAP, s); }

void preview_filter(Preview *p) {
    p->match_count = 0;
    for (int i = 0; i < p->count; ++i) {
        PreviewBook *b = &p->books[i];
        if (!p->search.text[0] || strstr(b->title, p->search.text) ||
            strstr(b->author, p->search.text) || strstr(b->isbn, p->search.text))
            p->matches[p->match_count++] = i;
    }
    if (p->selected >= p->match_count) p->selected = p->match_count - 1;
    if (p->selected < 0) p->selected = 0;
}

void preview_init(Preview *p, Page page) {
    static const char *titles[] = { "C程序设计语言", "深入理解计算机系统", "操作系统概念",
        "算法导论", "计算机网络（第7版）", "数据结构（C语言版）", "中国文学史", "时间简史",
        "自动控制原理", "数字电子技术基础", "现代操作系统", "编译原理",
        "人工智能：一种现代方法", "线性代数", "高等数学", "信号与系统",
        "嵌入式系统设计", "数据库系统概论", "软件工程", "这是一本用于验证中文超长书名安全截断的测试图书" };
    static const char *authors[] = { "Brian W. Kernighan", "Randal E. Bryant", "Abraham Silberschatz",
        "Thomas H. Cormen", "谢希仁", "严蔚敏", "袁行霈", "史蒂芬·霍金" };
    Preview empty = {0};
    *p = empty;
    p->page = page;
    p->running = 1;
    p->count = 20;
    for (int i = 0; i < p->count; ++i) {
        text_copy(p->books[i].title, TEXT_CAP, titles[i]);
        text_copy(p->books[i].author, TEXT_CAP, authors[i % 8]);
        text_copy(p->books[i].isbn, 64, i == 0 ? "9787111128069" : "DEMO-SAMPLE");
        p->books[i].copies = 3;
        p->books[i].available = i % 5 == 2 ? 0 : 2;
    }
    text_copy(p->form[3].text, TEXT_CAP, "1");
    p->form[3].cursor = 1;
    preview_filter(p);
    message(p, "界面原型：仅使用内存样例，退出不保存，不访问正式 CSV。");
}

static void form_open(Preview *p) {
    for (int i = 0; i < 4; ++i) { p->form[i].text[0] = 0; p->form[i].cursor = 0; }
    text_copy(p->form[3].text, TEXT_CAP, "1");
    p->form[3].cursor = 1;
    p->focus = p->dirty_form = p->confirm_cancel = 0;
    p->page = PAGE_FORM;
    message(p, "填写完成后选择保存；数据只保留在本次原型会话。");
}

static void form_save(Preview *p) {
    int copies;
    for (int i = 0; i < 3; ++i) if (!p->form[i].text[0]) {
        p->focus = i; message(p, "错误：ISBN、书名与作者均不能为空。"); return;
    }
    if (strlen(p->form[0].text) >= 64) {
        p->focus = 0; message(p, "错误：ISBN 或演示编号最长 63 字节。"); return;
    }
    if (!positive_number(p->form[3].text, &copies)) {
        p->focus = 3; message(p, "错误：副本数必须是 1 到 2147483647 的整数。"); return;
    }
    if (p->count == MAX_BOOKS) { message(p, "错误：原型的 32 条样例容量已满。"); return; }
    for (int i = 0; i < p->count; ++i)
        if (strcmp(p->books[i].isbn, p->form[0].text) == 0) {
            p->focus = 0; message(p, "错误：该 ISBN 或演示编号已存在。"); return;
        }
    PreviewBook *b = &p->books[p->count++];
    text_copy(b->isbn, 64, p->form[0].text);
    text_copy(b->title, TEXT_CAP, p->form[1].text);
    text_copy(b->author, TEXT_CAP, p->form[2].text);
    b->copies = b->available = copies;
    b->suspended = 0;
    p->search.text[0] = 0; p->search.cursor = 0;
    preview_filter(p);
    p->selected = p->match_count - 1;
    p->page = PAGE_BOOKS; p->dirty_form = 0;
    message(p, "样例已加入内存列表；退出后消失，未写入正式数据。");
}

static int same_character(Event e, char a, char b) {
    return e.key == KEY_TEXT && !e.text[1] && (e.text[0] == a || e.text[0] == b);
}

static void cancel_form(Preview *p) {
    if (p->dirty_form) p->confirm_cancel = 1;
    else { p->page = PAGE_BOOKS; message(p, "已返回图书列表。"); }
}

void preview_event(Preview *p, Event e) {
    if (e.key == KEY_QUIT) { p->running = 0; return; }
    if (e.key == KEY_F1) { p->help = !p->help; return; }
    if (p->help) { if (e.key == KEY_ESC || e.key == KEY_ENTER) p->help = 0; return; }
    if (p->detail) { if (e.key == KEY_ESC || e.key == KEY_ENTER) p->detail = 0; return; }
    if (p->confirm_cancel) {
        if (same_character(e, 'y', 'Y')) {
            p->confirm_cancel = p->dirty_form = 0;
            p->page = PAGE_BOOKS; message(p, "已放弃未保存的原型输入。");
        } else if (same_character(e, 'n', 'N') || e.key == KEY_ESC) p->confirm_cancel = 0;
        return;
    }
    if (p->page == PAGE_BOOKS) {
        if (p->searching) {
            if (e.key == KEY_ENTER || e.key == KEY_ESC) { p->searching = 0; return; }
            if (e.key == KEY_TEXT) {
                if (!edit_insert(&p->search, e.text)) message(p, "无法输入：长度或字符不受支持。");
            } else edit_key(&p->search, e.key);
            p->selected = 0; preview_filter(p); return;
        }
        if (e.key == KEY_ESC) {
            p->page = PAGE_LOGIN; p->focus = 0;
            for (int i = 0; i < TEXT_CAP; ++i) p->login[1].text[i] = 0;
            p->login[1].cursor = 0;
            message(p, "已退出原型登录；Ctrl+Q 退出程序。");
        } else if (e.key == KEY_UP && p->selected > 0) --p->selected;
        else if (e.key == KEY_DOWN && p->selected + 1 < p->match_count) ++p->selected;
        else if (e.key == KEY_PGUP) p->selected = p->selected < PAGE_ROWS ? 0 : p->selected - PAGE_ROWS;
        else if (e.key == KEY_PGDN) {
            p->selected += PAGE_ROWS;
            if (p->selected >= p->match_count) p->selected = p->match_count ? p->match_count - 1 : 0;
        } else if (e.key == KEY_HOME) p->selected = 0;
        else if (e.key == KEY_END) p->selected = p->match_count ? p->match_count - 1 : 0;
        else if (same_character(e, 'k', 'K')) p->searching = 1;
        else if (same_character(e, 'c', 'C')) form_open(p);
        else if (e.key == KEY_F5) message(p, "内存样例已刷新；原型没有后台同步或文件写入。");
        else if (e.key == KEY_ENTER && p->match_count) p->detail = 1;
        else if (same_character(e, 'z', 'Z') && p->match_count) {
            PreviewBook *b = &p->books[p->matches[p->selected]];
            if (b->copies == 2147483647) message(p, "错误：副本数已达上限。");
            else { ++b->copies; ++b->available; message(p, "仅在内存样例中增加了一个副本。"); }
        } else if (same_character(e, 't', 'T') && p->match_count) {
            PreviewBook *b = &p->books[p->matches[p->selected]];
            b->suspended = !b->suspended; message(p, "仅在内存样例中切换了停借状态。");
        }
        return;
    }
    int fields = p->page == PAGE_LOGIN ? 2 : 4;
    int controls = fields + 2;
    Edit *edits = p->page == PAGE_LOGIN ? p->login : p->form;
    if (e.key == KEY_TAB || e.key == KEY_DOWN) p->focus = (p->focus + 1) % controls;
    else if (e.key == KEY_BACKTAB || e.key == KEY_UP) p->focus = (p->focus + controls - 1) % controls;
    else if (e.key == KEY_ESC) {
        if (p->page == PAGE_LOGIN) p->running = 0;
        else cancel_form(p);
    } else if (e.key == KEY_ENTER) {
        if (p->focus < fields) ++p->focus;
        else if (p->page == PAGE_FORM) {
            if (p->focus == fields) form_save(p); else cancel_form(p);
        } else if (p->focus == fields + 1) p->running = 0;
        else if (strcmp(p->login[0].text, "admin") || strcmp(p->login[1].text, "admin123"))
            message(p, "错误：演示账号 admin / admin123；此处未接入正式认证。");
        else {
            p->page = PAGE_BOOKS; p->focus = 0;
            for (int i = 0; i < TEXT_CAP; ++i) p->login[1].text[i] = 0;
            p->login[1].cursor = 0;
            message(p, "原型演示登录成功；所有数据仅在内存中。");
        }
    } else if (p->focus < fields) {
        int changed;
        if (e.key == KEY_TEXT) {
            changed = edit_insert(&edits[p->focus], e.text);
            if (!changed) message(p, "无法输入：字段已满，或包含不支持的字符。");
        } else changed = edit_key(&edits[p->focus], e.key);
        if (changed && p->page == PAGE_FORM) p->dirty_form = 1;
    }
}

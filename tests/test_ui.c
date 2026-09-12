#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "ui/widget.h"

static int passed_count = 0;
static int failed_count = 0;

static void check(bool condition, const char *name) {
    if (condition) {
        ++passed_count;
        printf("[PASS] %s\n", name);
    } else {
        ++failed_count;
        printf("[FAIL] %s\n", name);
    }
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    const UiRect rect = {10, 5, 20, 3};

    check(widget_contains_point(&rect, 10, 5), "mouse hits top-left edge");
    check(widget_contains_point(&rect, 29, 7), "mouse hits bottom-right edge");
    check(!widget_contains_point(&rect, 30, 7), "mouse rejects right outside");
    check(!widget_contains_point(&rect, 9, 5), "mouse rejects left outside");
    check(widget_text_display_width(L"图书A") == 5,
          "mixed Chinese and ASCII display width");
    check(widget_text_display_width(L"") == 0, "empty text display width");

    printf(
        "\nUI test result: %d passed, %d failed.\n",
        passed_count, failed_count
    );
    return failed_count == 0 ? 0 : 1;
}

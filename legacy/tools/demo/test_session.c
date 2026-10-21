#include "demo_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(bool valid, const char *label) {
    printf("[%s] %s\n", valid ? "PASS" : "FAIL", label);
    if (!valid) ++failures;
}
static void write_token(const wchar_t *root, const char *value) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\data\\readers.csv", root);
    FILE *file = _wfopen(path, L"wb");
    if (!file) exit(2);
    fputs(value, file); fclose(file);
}
static bool contains(const wchar_t *root, const char *value) {
    wchar_t path[MAX_PATH]; char buffer[4096] = {0};
    swprintf(path, MAX_PATH, L"%ls\\data\\readers.csv", root);
    FILE *file = _wfopen(path, L"rb");
    if (!file) return false;
    fread(buffer, 1, sizeof(buffer) - 1, file); fclose(file);
    return strstr(buffer, value) != NULL;
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    HANDLE a = NULL, b = NULL, c = NULL;
    wchar_t marker[MAX_PATH], missing[MAX_PATH];
    write_token(argv[1], "previous-session-reader");
    check(demo_session_begin(argv[1], &a), "first terminal starts");
    check(!contains(argv[1], "previous-session-reader") &&
           contains(argv[1], "demo_reader"), "first startup restores seed accounts");
    write_token(argv[1], "new-session-reader");
    check(demo_session_begin(argv[1], &b), "second terminal joins");
    check(contains(argv[1], "new-session-reader"), "second startup preserves registration");
    if (a) CloseHandle(a);
    check(demo_session_begin(argv[1], &c), "third terminal joins after first exits");
    check(contains(argv[1], "new-session-reader"), "remaining terminal keeps session alive");
    if (b) CloseHandle(b);
    if (c) CloseHandle(c);
    a = NULL;
    check(demo_session_begin(argv[1], &a), "next round starts after all handles close");
    check(!contains(argv[1], "new-session-reader") &&
           contains(argv[1], "demo_reader"), "next round clears registered account");
    if (a) CloseHandle(a);
    write_token(argv[1], "protected-data");
    swprintf(marker, MAX_PATH, L"%ls\\demo-mode.txt", argv[1]);
    swprintf(missing, MAX_PATH, L"%ls\\demo-mode.disabled", argv[1]);
    MoveFileW(marker, missing);
    a = NULL;
    check(!demo_session_begin(argv[1], &a) && contains(argv[1], "protected-data"),
          "missing demo marker rejects reset and preserves data");
    MoveFileW(missing, marker);
    swprintf(marker, MAX_PATH, L"%ls\\初始数据备份\\readers.csv", argv[1]);
    swprintf(missing, MAX_PATH, L"%ls\\初始数据备份\\readers.disabled", argv[1]);
    MoveFileW(marker, missing);
    check(!demo_session_begin(argv[1], &a) && contains(argv[1], "protected-data"),
          "missing template rejects reset and preserves data");
    MoveFileW(missing, marker);
    printf("Session lifecycle tests: %d failures\n", failures);
    return failures ? 1 : 0;
}

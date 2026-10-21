#include "demo_session.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static const wchar_t *const files[] = {
    L"books.csv", L"readers.csv", L"loans.csv", L"fines.csv", L"reservations.csv"
};

static bool path_join(wchar_t path[MAX_PATH], const wchar_t *root,
                      const wchar_t *directory, const wchar_t *file) {
    if (wcslen(root) + wcslen(directory) + wcslen(file) + 3 >= MAX_PATH)
        return false;
    return swprintf(path, MAX_PATH, L"%ls\\%ls\\%ls", root, directory, file) > 0;
}

bool demo_session_root(wchar_t root[MAX_PATH]) {
    DWORD length = GetModuleFileNameW(NULL, root, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    wchar_t *separator = wcsrchr(root, L'\\');
    if (!separator) return false;
    *separator = L'\0';
    separator = wcsrchr(root, L'\\');
    if (!separator || _wcsicmp(separator + 1, L"build") != 0) return false;
    *separator = L'\0';
    return true;
}

static bool validate_demo(const wchar_t *root) {
    wchar_t path[MAX_PATH];
    static const char marker[] = "library-demo-reset-v1";
    char contents[sizeof(marker)] = {0};
    DWORD read = 0;
    if (wcslen(root) + 15 >= MAX_PATH) return false;
    swprintf(path, MAX_PATH, L"%ls\\demo-mode.txt", root);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool valid = ReadFile(file, contents, sizeof(marker) - 1, &read, NULL) &&
                 read == sizeof(marker) - 1 &&
                 memcmp(contents, marker, sizeof(marker) - 1) == 0;
    CloseHandle(file);
    if (!valid) return false;
    const wchar_t *directories[] = {L"data", L"初始数据备份"};
    for (size_t i = 0; i < 2; ++i) {
        swprintf(path, MAX_PATH, L"%ls\\%ls", root, directories[i]);
        DWORD attributes = GetFileAttributesW(path);
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (!path_join(path, root, L"初始数据备份", files[i])) return false;
        DWORD attributes = GetFileAttributesW(path);
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return false;
        if (!path_join(path, root, L"data", files[i])) return false;
        attributes = GetFileAttributesW(path);
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return false;
    }
    return true;
}

static bool restore_seed(const wchar_t *root) {
    wchar_t source[MAX_PATH], target[MAX_PATH], staged[MAX_PATH];
    /* Stage every template first. A missing/unreadable template changes nothing. */
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (!path_join(source, root, L"初始数据备份", files[i]) ||
            !path_join(target, root, L"data", files[i]) ||
            wcslen(target) + 11 >= MAX_PATH) return false;
        swprintf(staged, MAX_PATH, L"%ls.demo.tmp", target);
        if (!CopyFileW(source, staged, FALSE)) return false;
    }
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (!path_join(target, root, L"data", files[i])) return false;
        swprintf(staged, MAX_PATH, L"%ls.demo.tmp", target);
        if (!MoveFileExW(staged, target,
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return false;
    }
    return true;
}

bool demo_session_begin(const wchar_t *root, HANDLE *session) {
    wchar_t absolute[MAX_PATH], mutex_name[80];
    if (!root || !session) return false;
    *session = NULL;
    DWORD length = GetFullPathNameW(root, MAX_PATH, absolute, NULL);
    if (!length || length >= MAX_PATH || !validate_demo(absolute)) return false;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; absolute[i]; ++i) {
        hash ^= (uint16_t)towlower(absolute[i]);
        hash *= UINT64_C(1099511628211);
    }
    swprintf(mutex_name, 80, L"Local\\CosmicLibraryDemo-%016llx",
             (unsigned long long)hash);
    HANDLE handle = CreateMutexW(NULL, TRUE, mutex_name);
    DWORD creation = GetLastError();
    if (!handle) return false;
    if (creation == ERROR_ALREADY_EXISTS) {
        DWORD result = WaitForSingleObject(handle, 30000);
        if (result != WAIT_OBJECT_0) {
            if (result == WAIT_ABANDONED) ReleaseMutex(handle);
            CloseHandle(handle);
            return false;
        }
    } else if (!restore_seed(absolute)) {
        /* Caller exits immediately on failure. Waiting starters see abandonment. */
        CloseHandle(handle);
        return false;
    }
    ReleaseMutex(handle);
    *session = handle;
    return true;
}

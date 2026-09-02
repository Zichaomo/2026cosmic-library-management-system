/* 文件工具 - 通用工具层 */
#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

bool file_exists(const char *path) {
    FILE *f;
    if (!path) return false;
    f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

Result file_atomic_write(const char *path, const char *content, size_t len) {
    char tmp[1024];
    FILE *f;
    size_t w;
    int rc;
    if (!path || !content) return RES_ERR_INVALID;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return RES_ERR_IO;
    w = fwrite(content, 1, len, f);
    rc = fclose(f);
    if (w != len || rc != 0) return RES_ERR_IO;
    if (rename(tmp, path) != 0) return RES_ERR_IO;
    return RES_OK;
}

char *file_read_all(const char *path, size_t *out_len) {
    FILE *f;
    long sz;
    char *buf;
    size_t rd;
    if (out_len) *out_len = 0;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

Result file_ensure_dir(const char *dir) {
    char tmp[512];
    size_t len;
    char *p;
    if (!dir || !*dir) return RES_ERR_INVALID;
    len = strlen(dir);
    if (len >= sizeof tmp) return RES_ERR_INVALID;
    memcpy(tmp, dir, len + 1);
    /* 逐级创建（跳过已存在目录） */
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            _mkdir(tmp);
            *p = save;
        }
    }
    _mkdir(tmp);
    return RES_OK;
}

/* 公共类型 - 基础层 */
#include "types.h"

#include <stdlib.h>
#include <string.h>

#define DYNARRAY_INIT_CAP 128 /* 初始容量（PRD §6.5） */

void dynarray_init(DynArray *a, size_t elem_size) {
    if (!a) return;
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
    a->elem_size = elem_size;
}

void dynarray_free(DynArray *a) {
    if (!a) return;
    free(a->data);
    a->data = NULL;
    a->len = a->cap = 0;
}

Result dynarray_push(DynArray *a, const void *elem) {
    if (!a || !elem || a->elem_size == 0) return RES_ERR_INVALID;
    if (a->len == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : DYNARRAY_INIT_CAP;
        void *nd = realloc(a->data, nc * a->elem_size);
        if (!nd) return RES_ERR_NO_MEM;
        a->data = nd;
        a->cap = nc;
    }
    memcpy((char *)a->data + a->len * a->elem_size, elem, a->elem_size);
    a->len++;
    return RES_OK;
}

void *dynarray_get(const DynArray *a, size_t index) {
    if (!a || index >= a->len) return NULL;
    return (char *)a->data + index * a->elem_size;
}

#include <stdlib.h>
#include <string.h>

#include "db.h"

#define _pure_ __attribute__((pure))

#define CB(X) (1 & (intptr_t)(X))
#define CB_PTR(X) (uint8_t *)(1 | (intptr_t)(X))
#define CB_NODE(X) (struct node *)(1 ^ (intptr_t)(X))

#define CB_1(x) (__builtin_expect((x), 1))
#define CB_0(x) (__builtin_expect((x), 0))
#define CLZ(x) (__builtin_clz(x))

struct node {
    uint8_t *child[2];
    uint32_t point;
};

_pure_ static inline size_t
str_cmp(const char *restrict sa, const char *restrict sb)
{
    if (!sa || !sb)
        return 1;

    size_t i = 0;

    while (sa[i] == sb[i])
        if (!sa[i++])
            return 0;

    return i + 1;
}

_pure_ static inline size_t
str_len(const char *restrict str)
{
    if (!str)
        return 0;

    return strlen(str);
}

_pure_ static inline size_t
db_size(const uint8_t *a)
{
    return (a[0] ?: str_len((char *)a + 1)) + 1;
}

_pure_ static inline size_t
db_cmp(const uint8_t *a, const uint8_t *b)
{
    const size_t size = a[0];

    if (size != b[0])
        return 1;

    if (!size) {
        size_t i = str_cmp((char *)a + 1, (char *)b + 1);
        return i ? i + 1 : 0;
    }

    for (size_t i = 1; i <= size; i++) {
        if (a[i] != b[i])
            return i + 1;
    }

    return 0;
}

_pure_ static inline int
cb_dir(const uint32_t point, uint8_t *data, const size_t size)
{
    const size_t pos = point >> 8;

    if (pos >= size)
        return 0;

    return ((point | data[pos]) & 255) == 255;
}

uint8_t *
cb_search(uint8_t **p, uint8_t *data)
{
    if (CB_0(!*p))
        return NULL;

    uint8_t *r = *p;
    const size_t size = db_size(data);

    while (CB(r)) {
        struct node *node = CB_NODE(r);
        r = node->child[cb_dir(node->point, data, size)];
    }

    if (!db_cmp(r, data))
        return r;

    return NULL;
}

uint8_t *
cb_insert(uint8_t **p, uint8_t *data)
{
    if (CB_0(CB(data)))
        return NULL;

    if (CB_0(!*p)) {
        *p = data;
        return data;
    }

    uint8_t *r = *p;
    size_t size = db_size(data);

    while (CB(r)) {
        struct node *node = CB_NODE(r);
        r = node->child[cb_dir(node->point, data, size)];
    }

    const size_t diff = db_cmp(r, data);

    if (CB_0(!diff))
        return r;

    const size_t pos = diff - 1;
    const uint8_t mask = ~((1u << 31) >> CLZ(r[pos] ^ data[pos]));
    const size_t point = (pos << 8) | mask;

    while (CB(*p)) {
        struct node *node = CB_NODE(*p);

        if (node->point > point)
            break;

        p = node->child + cb_dir(node->point, data, size);
    }

    struct node *node = malloc(sizeof(struct node));

    if (CB_0(!node))
        return NULL;

    const int dir = (mask | r[pos]) == 255;

    node->child[dir] = *p;
    node->child[1 - dir] = data;
    node->point = point;

    *p = CB_PTR(node);

    return data;
}

uint8_t *
cb_remove(uint8_t **p, uint8_t *data)
{
    if (CB_0(!*p))
        return NULL;

    const size_t size = db_size(data);

    uint8_t **p_old = NULL;
    struct node *node = NULL;
    int dir = 0;

    while (CB(*p)) {
        p_old = p;
        node = CB_NODE(*p);
        dir = cb_dir(node->point, data, size);
        p = node->child + dir;
    }

    if (CB_0(db_cmp(data, *p)))
        return NULL;

    uint8_t *r = *p;

    if (p_old) {
        *p_old = node->child[1 - dir];
        free(node);
    } else {
        *p = NULL;
    }

    return r;
}

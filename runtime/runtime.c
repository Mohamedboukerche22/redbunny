/*
 * RedBunny native runtime.
 *
 * Every function here is called directly from generated LLVM IR. Keep the
 * signatures in sync with codegen.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

void redb_write(const char *str)
{
    fputs(str, stdout);
}

void redb_writeln(const char *str)
{
    puts(str);
}

void redb_write_int(long long value)
{
    printf("%lld", value);
}

void redb_write_double(double value)
{
    printf("%g", value);
}

void redb_write_bool(int value)
{
    fputs(value ? "true" : "false", stdout);
}

void redb_newline(void)
{
    putchar('\n');
}

char *redb_str_concat(const char *left, const char *right)
{
    size_t llen = strlen(left);
    size_t rlen = strlen(right);
    char *out = malloc(llen + rlen + 1);

    if (!out) {
        fputs("redb: out of memory\n", stderr);
        exit(1);
    }
    memcpy(out, left, llen);
    memcpy(out + llen, right, rlen + 1);
    return out;
}

int redb_str_eq(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

void *redb_alloc(size_t size)
{
    void *p = malloc(size ? size : 1);

    if (!p) {
        fputs("redb: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

void redb_writeformat(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}


/* ---- arrays ----------------------------------------------------------
 * An array is a heap object. Its elements are stored in one contiguous
 * buffer. If the element is itself an array, each slot holds a pointer
 * to another RedbArray (nest > 0).
 */
typedef struct RedbArray {
    int64_t len;
    int64_t cap;
    int64_t nest;       /* number of array levels nested inside this one */
    int64_t leaf_size;  /* byte size of the innermost scalar element */
    int64_t elem_size;  /* byte size of one slot in data */
    char *data;
} RedbArray;

static void array_oom(void)
{
    fputs("redb: out of memory\n", stderr);
    exit(1);
}

static void array_fill_children(RedbArray *a, int64_t from, int64_t to);

RedbArray *redb_array_new(int64_t len, int64_t nest, int64_t leaf_size)
{
    RedbArray *a;

    if (len < 0) {
        fputs("redb: runtime error: negative array size\n", stderr);
        exit(1);
    }
    a = malloc(sizeof(*a));
    if (!a)
        array_oom();

    a->len = len;
    a->cap = len;
    a->nest = nest;
    a->leaf_size = leaf_size;
    a->elem_size = nest > 0 ? (int64_t)sizeof(void *) : leaf_size;
    a->data = calloc(len > 0 ? (size_t)len : 1, (size_t)a->elem_size);
    if (!a->data)
        array_oom();

    if (nest > 0)
        array_fill_children(a, 0, len);
    return a;
}

static void array_fill_children(RedbArray *a, int64_t from, int64_t to)
{
    int64_t i;

    for (i = from; i < to; i++) {
        RedbArray *child = redb_array_new(0, a->nest - 1, a->leaf_size);

        memcpy(a->data + i * a->elem_size, &child, sizeof(child));
    }
}

/* Returns the address of element 'index' after a bounds check. */
void *redb_array_at(RedbArray *a, int64_t index)
{
    if (!a) {
        fputs("redb: runtime error: use of an uninitialized array\n", stderr);
        exit(1);
    }
    if (index < 0 || index >= a->len) {
        fprintf(stderr,
                "redb: runtime error: index %lld out of range for array of size %lld\n",
                (long long)index, (long long)a->len);
        exit(1);
    }
    return a->data + index * a->elem_size;
}

int64_t redb_array_size(RedbArray *a)
{
    return a ? a->len : 0;
}

void redb_array_resize(RedbArray *a, int64_t n)
{
    int64_t old;

    if (!a) {
        fputs("redb: runtime error: resize of an uninitialized array\n", stderr);
        exit(1);
    }
    if (n < 0) {
        fputs("redb: runtime error: negative size in resize()\n", stderr);
        exit(1);
    }

    old = a->len;
    if (n > a->cap) {
        char *grown = realloc(a->data, (size_t)n * (size_t)a->elem_size);

        if (!grown)
            array_oom();
        a->data = grown;
        a->cap = n;
    }
    if (n > old) {
        memset(a->data + old * a->elem_size, 0, (size_t)((n - old) * a->elem_size));
        a->len = n;
        if (a->nest > 0)
            array_fill_children(a, old, n);
    }
    a->len = n;
}

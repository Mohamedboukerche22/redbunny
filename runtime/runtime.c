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

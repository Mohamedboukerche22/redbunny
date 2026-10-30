#include "redb.h"

void *redb_xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (!p) {
        fprintf(stderr, "redb: out of memory\n");
        exit(1);
    }
    return p;
}

void *redb_xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);
    if (!p) {
        fprintf(stderr, "redb: out of memory\n");
        exit(1);
    }
    return p;
}

char *redb_xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = redb_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

void redb_error(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "redb: error");
    if (line > 0)
        fprintf(stderr, ":%d", line);
    fprintf(stderr, ": ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

const char *redb_type_name(RedbType type)
{
    switch (type) {
    case TY_VOID:   return "void";
    case TY_INT:    return "int";
    case TY_FLOAT:  return "float";
    case TY_DOUBLE: return "double";
    case TY_STR:    return "str";
    case TY_BOOL:   return "bool";
    case TY_FUNC:   return "function";
    default:        return "unknown";
    }
}

RedbType redb_parse_type_name(const char *name)
{
    if (!strcmp(name, "int"))    return TY_INT;
    if (!strcmp(name, "float"))  return TY_FLOAT;
    if (!strcmp(name, "double")) return TY_DOUBLE;
    if (!strcmp(name, "str"))    return TY_STR;
    if (!strcmp(name, "void"))   return TY_VOID;
    return TY_UNKNOWN;
}

char *redb_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long len;

    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    buf = redb_xmalloc((size_t)len + 1);
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int redb_write_file(const char *path, const char *data)
{
    FILE *f = fopen(path, "wb");
    size_t len = strlen(data);

    if (!f)
        return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

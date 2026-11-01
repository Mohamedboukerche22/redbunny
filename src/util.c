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

#define REDB_MAX_ARRAY_TYPES 256
static RedbType array_elems[REDB_MAX_ARRAY_TYPES];
static int array_count;

/* Returns the (interned) type "array<elem>". */
RedbType redb_array_of(RedbType elem)
{
    int i;

    for (i = 0; i < array_count; i++) {
        if (array_elems[i] == elem)
            return (RedbType)(REDB_ARRAY_BASE + i);
    }
    if (array_count >= REDB_MAX_ARRAY_TYPES) {
        fprintf(stderr, "redb: error: too many distinct array types\n");
        exit(1);
    }
    array_elems[array_count] = elem;
    array_count++;
    return (RedbType)(REDB_ARRAY_BASE + array_count - 1);
}

int redb_is_array(RedbType type)
{
    return (int)type >= REDB_ARRAY_BASE;
}

RedbType redb_elem_type(RedbType type)
{
    if (!redb_is_array(type))
        return TY_UNKNOWN;
    return array_elems[(int)type - REDB_ARRAY_BASE];
}

const char *redb_type_name(RedbType type)
{
    static char names[4][160];
    static int next;
    char *out;

    if (redb_is_array(type)) {
        out = names[next];
        next = (next + 1) % 4;
        snprintf(out, sizeof(names[0]), "array<%s>",
                 redb_type_name(redb_elem_type(type)));
        return out;
    }
    switch (type) {
    case TY_VOID:   return "void";
    case TY_INT:    return "int";
    case TY_FLOAT:  return "float";
    case TY_DOUBLE: return "double";
    case TY_STR:    return "str";
    case TY_BOOL:   return "bool";
    case TY_FUNC:   return "function";
    case TY_AUTO:   return "auto";
    default:        return "unknown";
    }
}

RedbType redb_parse_type_name(const char *name)
{
    if (!strcmp(name, "int"))    return TY_INT;
    if (!strcmp(name, "float"))  return TY_FLOAT;
    if (!strcmp(name, "double")) return TY_DOUBLE;
    if (!strcmp(name, "bool"))   return TY_BOOL;
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

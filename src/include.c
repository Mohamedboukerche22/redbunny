#include "redb.h"

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_stdlib_dir[PATH_MAX];

void redb_set_stdlib_dir(const char *dir)
{
    if (!dir || !dir[0]) {
        g_stdlib_dir[0] = '\0';
        return;
    }
    snprintf(g_stdlib_dir, sizeof(g_stdlib_dir), "%s", dir);
}

static char *dir_of(const char *path)
{
    char *copy = redb_xstrdup(path);
    char *slash = strrchr(copy, '/');

    if (slash) {
        *slash = '\0';
    } else {
        free(copy);
        copy = redb_xstrdup(".");
    }
    return copy;
}

static char *include_path(const char *base_dir, const char *name)
{
    size_t len = strlen(name);
    size_t size = strlen(base_dir) + len + 8;
    char *path = redb_xmalloc(size);

    if (len >= 5 && !strcmp(name + len - 5, ".redb"))
        snprintf(path, size, "%s/%s", base_dir, name);
    else
        snprintf(path, size, "%s/%s.redb", base_dir, name);
    return path;
}

static int expand_program(ASTNode *program, const char *base_dir, int depth)
{
    ASTNode *out;
    size_t i;

    if (depth > 32) {
        redb_error(program->line, "include nesting is too deep (possible cycle)");
        return -1;
    }

    out = create_node(AST_PROGRAM, program->line);

    for (i = 0; i < program->child_count; i++) {
        ASTNode *child = program->children[i];

        if (child->type != AST_INCLUDE) {
            add_child(out, child);
            continue;
        }

        if (!strcmp(child->string_val, "std")) {
            free_ast(child);
            continue;
        }

        {
            char *path = include_path(base_dir, child->string_val);
            char *source = redb_read_file(path);
            char *fallback = NULL;
            char *inc_dir;
            ASTNode *included;
            size_t j;

            if (!source && g_stdlib_dir[0]) {
                fallback = include_path(g_stdlib_dir, child->string_val);
                source = redb_read_file(fallback);
                if (source) {
                    free(path);
                    path = fallback;
                    fallback = NULL;
                }
            }

            if (!source) {
                if (fallback) {
                    redb_error(child->line,
                               "cannot include '%s' (looked for %s and %s)",
                               child->string_val, path, fallback);
                    free(fallback);
                } else {
                    redb_error(child->line, "cannot include '%s' (looked for %s)",
                               child->string_val, path);
                }
                free(path);
                return -1;
            }

            if (redb_lex(source, path) != 0) {
                free(source);
                free(path);
                return -1;
            }
            free(source);

            included = redb_parse(path);
            inc_dir = dir_of(path);
            if (expand_program(included, inc_dir, depth + 1) != 0) {
                free(inc_dir);
                free(path);
                return -1;
            }
            free(inc_dir);
            free(path);

            for (j = 0; j < included->child_count; j++)
                add_child(out, included->children[j]);

            /* The children now belong to 'out'. */
            included->child_count = 0;
            free_ast(included);
            free_ast(child);
        }
    }

    free(program->children);
    program->children = out->children;
    program->child_count = out->child_count;
    program->child_cap = out->child_cap;
    free(out);
    return 0;
}

int redb_expand_includes(ASTNode *program, const char *main_file)
{
    char *base_dir = dir_of(main_file);
    int rc = expand_program(program, base_dir, 0);

    free(base_dir);
    return rc;
}

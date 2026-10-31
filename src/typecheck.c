#include "redb.h"

#define MAX_SCOPE_DEPTH 128
#define MAX_SYMBOLS 512

typedef struct {
    char name[128];
    RedbType type;
    int is_func;
    int param_count;
    RedbType params[REDB_MAX_PARAMS];
    RedbType ret;
} TSym;

typedef struct {
    TSym syms[MAX_SYMBOLS];
    int count;
} TScope;

static TScope scopes[MAX_SCOPE_DEPTH];
static int scope_top;
static int scope_barrier;

static RedbType current_ret;
static int current_is_main;
static int loop_depth;

static void typecheck_stmt(ASTNode *node);
static RedbType infer_expr(ASTNode *node);

static void tc_fail(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "redb: error:%d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void push_scope(void)
{
    if (scope_top >= MAX_SCOPE_DEPTH)
        tc_fail(0, "scope nesting too deep");
    memset(&scopes[scope_top], 0, sizeof(scopes[scope_top]));
    scope_top++;
}

static void pop_scope(void)
{
    if (scope_top > 0)
        scope_top--;
}

static TSym *lookup(const char *name)
{
    int i, j;

    for (i = scope_top - 1; i >= 0; i--) {
        if (i < scope_barrier && i != 0)
            continue;
        for (j = scopes[i].count - 1; j >= 0; j--) {
            if (!strcmp(scopes[i].syms[j].name, name))
                return &scopes[i].syms[j];
        }
    }
    return NULL;
}

static TSym *lookup_current(const char *name)
{
    TScope *scope = &scopes[scope_top - 1];
    int i;

    for (i = 0; i < scope->count; i++) {
        if (!strcmp(scope->syms[i].name, name))
            return &scope->syms[i];
    }
    return NULL;
}

static TSym *declare(const char *name, RedbType type, int line)
{
    TScope *scope = &scopes[scope_top - 1];
    TSym *sym;

    if (lookup_current(name))
        tc_fail(line, "'%s' is already declared in this scope", name);
    if (scope->count >= MAX_SYMBOLS)
        tc_fail(line, "too many symbols in scope");

    sym = &scope->syms[scope->count++];
    memset(sym, 0, sizeof(*sym));
    snprintf(sym->name, sizeof(sym->name), "%s", name);
    sym->type = type;
    return sym;
}

static int is_numeric(RedbType type)
{
    return type == TY_INT || type == TY_FLOAT || type == TY_DOUBLE;
}

static int is_truthy(RedbType type)
{
    return type == TY_BOOL || is_numeric(type);
}

static RedbType promote(RedbType a, RedbType b)
{
    if (a == TY_DOUBLE || b == TY_DOUBLE)
        return TY_DOUBLE;
    if (a == TY_FLOAT || b == TY_FLOAT)
        return TY_FLOAT;
    return TY_INT;
}

static int assignable(RedbType from, RedbType to)
{
    if (from == to)
        return 1;
    if (is_numeric(from) && is_numeric(to))
        return 1;
    if (to == TY_INT && from == TY_BOOL)
        return 1;
    if (to == TY_BOOL && from == TY_INT)
        return 1;
    return 0;
}

static void add_format_literal(ASTNode *format, const char *text)
{
    ASTNode *lit;

    if (!text[0])
        return;
    lit = create_node(AST_STRING_LIT, format->line);
    snprintf(lit->string_val, sizeof(lit->string_val), "%s", text);
    lit->inferred = TY_STR;
    add_child(format, lit);
}

static void expand_format(ASTNode *node)
{
    const char *t = node->string_val;
    char literal[REDB_LEXEME_MAX];
    size_t i = 0, l = 0;

    while (t[i]) {
        if (t[i] == '{' && t[i + 1] == '{') {
            if (l + 1 < sizeof(literal))
                literal[l++] = '{';
            i += 2;
            continue;
        }
        if (t[i] == '}' && t[i + 1] == '}') {
            if (l + 1 < sizeof(literal))
                literal[l++] = '}';
            i += 2;
            continue;
        }
        if (t[i] == '{') {
            char name[128];
            size_t n = 0;
            TSym *sym;
            ASTNode *id;

            literal[l] = '\0';
            add_format_literal(node, literal);
            l = 0;
            i++;

            while (t[i] && t[i] != '}') {
                if (n + 1 < sizeof(name))
                    name[n++] = t[i];
                i++;
            }
            if (t[i] != '}')
                tc_fail(node->line, "unterminated '{' in writeformat template");
            name[n] = '\0';
            i++;

            if (!n)
                tc_fail(node->line, "empty '{}' placeholder in writeformat template");

            sym = lookup(name);
            if (!sym)
                tc_fail(node->line, "unknown variable '%s' in writeformat template", name);
            if (sym->is_func)
                tc_fail(node->line, "cannot interpolate function '%s'", name);

            id = create_node(AST_IDENT, node->line);
            snprintf(id->name, sizeof(id->name), "%s", name);
            id->inferred = sym->type;
            add_child(node, id);
            continue;
        }
        if (t[i] == '}')
            tc_fail(node->line, "unmatched '}' in writeformat template (use '}}')");

        if (l + 1 < sizeof(literal))
            literal[l++] = t[i];
        i++;
    }

    literal[l] = '\0';
    add_format_literal(node, literal);
    node->inferred = TY_VOID;
}

static RedbType infer_expr(ASTNode *node)
{
    RedbType type = TY_UNKNOWN;

    if (!node)
        return TY_VOID;

    switch (node->type) {
    case AST_INT_LIT:
        type = TY_INT;
        break;

    case AST_FLOAT_LIT:
        type = TY_DOUBLE;
        break;

    case AST_STRING_LIT:
        type = TY_STR;
        break;

    case AST_BOOL_LIT:
        type = TY_BOOL;
        break;

    case AST_IDENT: {
        TSym *sym = lookup(node->name);

        if (!sym)
            tc_fail(node->line, "unknown identifier '%s'", node->name);
        if (sym->is_func)
            tc_fail(node->line, "function '%s' cannot be used as a value", node->name);
        type = sym->type;
        break;
    }

    case AST_UNARY: {
        RedbType operand = infer_expr(node->children[0]);

        if (!strcmp(node->string_val, "-")) {
            if (!is_numeric(operand))
                tc_fail(node->line, "unary '-' requires a numeric operand, got %s",
                        redb_type_name(operand));
            type = operand;
        } else {
            if (!is_truthy(operand))
                tc_fail(node->line, "'!' requires a bool or numeric operand, got %s",
                        redb_type_name(operand));
            type = TY_BOOL;
        }
        break;
    }

    case AST_BINARY: {
        RedbType left = infer_expr(node->children[0]);
        RedbType right = infer_expr(node->children[1]);
        const char *op = node->string_val;

        if (!strcmp(op, "+") && left == TY_STR && right == TY_STR) {
            type = TY_STR;
            break;
        }
        if (!strcmp(op, "==") || !strcmp(op, "!=")) {
            if (left == TY_STR && right == TY_STR) {
                type = TY_BOOL;
                break;
            }
            if (!is_numeric(left) || !is_numeric(right))
                tc_fail(node->line, "operator '%s' requires matching numeric or string operands",
                        op);
            type = TY_BOOL;
            break;
        }
        if (!strcmp(op, "&&") || !strcmp(op, "||")) {
            if (!is_truthy(left) || !is_truthy(right))
                tc_fail(node->line, "operator '%s' requires bool or numeric operands", op);
            type = TY_BOOL;
            break;
        }
        if (!strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=")) {
            if (!is_numeric(left) || !is_numeric(right))
                tc_fail(node->line, "operator '%s' requires numeric operands", op);
            type = TY_BOOL;
            break;
        }
        if (!is_numeric(left) || !is_numeric(right))
            tc_fail(node->line, "operator '%s' requires numeric operands, got %s and %s",
                    op, redb_type_name(left), redb_type_name(right));
        type = promote(left, right);
        break;
    }

    case AST_CALL: {
        TSym *sym = lookup(node->name);
        size_t i;

        if (!sym || !sym->is_func)
            tc_fail(node->line, "unknown function '%s'", node->name);
        if ((int)node->child_count != sym->param_count)
            tc_fail(node->line, "function '%s' expects %d argument(s), got %zu",
                    node->name, sym->param_count, node->child_count);

        for (i = 0; i < node->child_count; i++) {
            RedbType arg = infer_expr(node->children[i]);

            if (!assignable(arg, sym->params[i]))
                tc_fail(node->line, "argument %zu of '%s' expects %s, got %s",
                        i + 1, node->name, redb_type_name(sym->params[i]),
                        redb_type_name(arg));
        }
        type = sym->ret;
        break;
    }

    case AST_CALL_WRITE:
    case AST_CALL_WRITELN: {
        RedbType arg = infer_expr(node->children[0]);

        if (arg == TY_VOID)
            tc_fail(node->line, "cannot print a void value");
        if (redb_is_array(arg))
            tc_fail(node->line, "cannot print an array; print one element with arr[i]");
        type = TY_VOID;
        break;
    }

    case AST_INDEX: {
        RedbType base = infer_expr(node->children[0]);
        RedbType index = infer_expr(node->children[1]);

        if (!redb_is_array(base))
            tc_fail(node->line, "cannot index a value of type %s", redb_type_name(base));
        if (index != TY_INT)
            tc_fail(node->line, "array index must be int, got %s", redb_type_name(index));
        type = redb_elem_type(base);
        break;
    }

    case AST_METHOD: {
        RedbType base = infer_expr(node->children[0]);

        if (!redb_is_array(base))
            tc_fail(node->line, "cannot call '%s' on a value of type %s",
                    node->name, redb_type_name(base));
        if (!strcmp(node->name, "size")) {
            if (node->child_count != 1)
                tc_fail(node->line, "size() takes no arguments");
            type = TY_INT;
        } else if (!strcmp(node->name, "resize")) {
            if (node->child_count != 2)
                tc_fail(node->line, "resize() takes exactly one argument");
            if (infer_expr(node->children[1]) != TY_INT)
                tc_fail(node->line, "resize() expects an int size");
            type = TY_VOID;
        } else {
            tc_fail(node->line, "arrays have no method '%s'", node->name);
        }
        break;
    }

    case AST_CALL_FORMAT:
        expand_format(node);
        type = TY_VOID;
        break;

    default:
        tc_fail(node->line, "invalid expression node %d", (int)node->type);
        break;
    }

    node->inferred = type;
    return type;
}

static void typecheck_function(ASTNode *fn)
{
    RedbType saved_ret = current_ret;
    int saved_main = current_is_main;
    int saved_loop = loop_depth;
    int saved_top = scope_top;
    int saved_barrier = scope_barrier;
    int i;

    /* Functions see globals and their arguments, not the caller's locals. */
    scope_barrier = saved_top;
    push_scope();

    for (i = 0; i < fn->param_count; i++) {
        char argname[32];

        snprintf(argname, sizeof(argname), "arg%d", i + 1);
        declare(argname, fn->param_types[i], fn->line);
    }

    current_ret = fn->ret_type;
    current_is_main = fn->is_main;
    loop_depth = 0;

    typecheck_stmt(fn->children[0]);

    scope_top = saved_top;
    scope_barrier = saved_barrier;
    current_ret = saved_ret;
    current_is_main = saved_main;
    loop_depth = saved_loop;
    fn->inferred = TY_FUNC;
}

static void typecheck_stmt(ASTNode *node)
{
    size_t i;

    if (!node)
        return;

    switch (node->type) {
    case AST_BLOCK:
        push_scope();
        for (i = 0; i < node->child_count; i++)
            typecheck_stmt(node->children[i]);
        pop_scope();
        node->inferred = TY_VOID;
        break;

    case AST_VAR_DECL: {
        RedbType value = infer_expr(node->children[0]);
        TSym *sym = lookup(node->name);

        if (value == TY_VOID)
            tc_fail(node->line, "cannot assign a void value to '%s'", node->name);

        if (sym) {
            if (sym->is_func)
                tc_fail(node->line, "cannot assign to function '%s'", node->name);
            if (!assignable(value, sym->type))
                tc_fail(node->line, "cannot assign %s to '%s' of type %s",
                        redb_type_name(value), node->name, redb_type_name(sym->type));
            node->inferred = sym->type;
            node->bool_val = 1; /* assignment to existing variable */
        } else {
            declare(node->name, value, node->line);
            node->inferred = value;
            node->bool_val = 0; /* fresh declaration */
        }
        break;
    }

    case AST_FUNC_DECL:
        typecheck_function(node);
        break;

    case AST_FOR_LOOP: {
        RedbType start = infer_expr(node->children[0]);
        RedbType end = infer_expr(node->children[1]);

        if (!is_numeric(start) || !is_numeric(end))
            tc_fail(node->line, "for range bounds must be numeric");

        push_scope();
        declare(node->name, TY_INT, node->line);
        loop_depth++;
        typecheck_stmt(node->children[2]);
        loop_depth--;
        pop_scope();
        node->inferred = TY_VOID;
        break;
    }

    case AST_WHILE_LOOP: {
        RedbType cond = infer_expr(node->children[0]);

        if (!is_truthy(cond))
            tc_fail(node->line, "while condition must be bool or numeric");
        loop_depth++;
        typecheck_stmt(node->children[1]);
        loop_depth--;
        node->inferred = TY_VOID;
        break;
    }

    case AST_IF: {
        RedbType cond = infer_expr(node->children[0]);

        if (!is_truthy(cond))
            tc_fail(node->line, "if condition must be bool or numeric");
        typecheck_stmt(node->children[1]);
        if (node->child_count > 2)
            typecheck_stmt(node->children[2]);
        node->inferred = TY_VOID;
        break;
    }

    case AST_RETURN:
        if (node->child_count > 0) {
            RedbType value = infer_expr(node->children[0]);

            if (current_ret == TY_VOID)
                tc_fail(node->line, "cannot return a value from a void function");
            if (!assignable(value, current_ret))
                tc_fail(node->line, "return type mismatch: expected %s, got %s",
                        redb_type_name(current_ret), redb_type_name(value));
        } else if (current_ret != TY_VOID && !current_is_main) {
            tc_fail(node->line, "function must return a value of type %s",
                    redb_type_name(current_ret));
        }
        node->inferred = TY_VOID;
        break;

    case AST_BREAK:
    case AST_CONTINUE:
        if (loop_depth == 0)
            tc_fail(node->line, "'%s' outside of a loop",
                    node->type == AST_BREAK ? "break" : "continue");
        node->inferred = TY_VOID;
        break;

    case AST_ARRAY_DECL: {
        RedbType declared = node->ret_type;
        RedbType size = infer_expr(node->children[0]);

        if (size != TY_INT)
            tc_fail(node->line, "array size must be int, got %s", redb_type_name(size));
        if (node->has_init) {
            RedbType init = infer_expr(node->children[1]);

            if (redb_elem_type(declared) == TY_AUTO) {
                if (!redb_is_array(init))
                    tc_fail(node->line, "array<auto> '%s' must be initialized with an array",
                            node->name);
                declared = init;          /* element type comes from the initializer */
            } else if (init != declared) {
                tc_fail(node->line, "cannot initialize '%s' of type %s with %s",
                        node->name, redb_type_name(declared), redb_type_name(init));
            }
        } else if (redb_elem_type(declared) == TY_AUTO) {
            declared = redb_array_of(TY_INT);  /* no initializer: default to int */
        }
        declare(node->name, declared, node->line);
        node->inferred = declared;
        break;
    }

    case AST_INDEX_ASSIGN: {
        RedbType target = infer_expr(node->children[0]);
        RedbType value = infer_expr(node->children[1]);

        if (value == TY_VOID)
            tc_fail(node->line, "cannot assign a void value to an array element");
        if (!assignable(value, target))
            tc_fail(node->line, "cannot assign %s to an array element of type %s",
                    redb_type_name(value), redb_type_name(target));
        node->inferred = target;
        break;
    }

    case AST_INCLUDE:
        node->inferred = TY_VOID;
        break;

    default:
        infer_expr(node);
        break;
    }
}

static void register_functions(ASTNode *node)
{
    size_t i;

    if (!node)
        return;

    if (node->type == AST_FUNC_DECL && !node->is_main) {
        TSym *sym;

        if (lookup_current(node->name))
            tc_fail(node->line, "function '%s' is already defined", node->name);
        sym = declare(node->name, TY_FUNC, node->line);
        sym->is_func = 1;
        sym->param_count = node->param_count;
        sym->ret = node->ret_type;
        memcpy(sym->params, node->param_types, sizeof(sym->params));
    }

    for (i = 0; i < node->child_count; i++)
        register_functions(node->children[i]);
}

void redb_typecheck(ASTNode *ast)
{
    size_t i;

    scope_top = 0;
    scope_barrier = 0;
    push_scope(); /* global scope */

    current_ret = TY_VOID;
    current_is_main = 0;
    loop_depth = 0;

    register_functions(ast);

    for (i = 0; i < ast->child_count; i++)
        typecheck_stmt(ast->children[i]);
}

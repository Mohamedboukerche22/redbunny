#include "redb.h"

static void set_name(ASTNode *node, const char *name)
{
    size_t n = strlen(name);

    if (n >= sizeof(node->name))
        n = sizeof(node->name) - 1;
    memcpy(node->name, name, n);
    node->name[n] = '\0';
}

static Token *p_tokens;
static size_t p_count;
static size_t p_pos;
static const char *p_file;

static ASTNode *parse_statement(void);
static ASTNode *parse_block(void);
static ASTNode *parse_expression(void);
static ASTNode *parse_if_statement(void);

ASTNode *create_node(ASTNodeType type, int line)
{
    ASTNode *node = redb_xmalloc(sizeof(*node));

    memset(node, 0, sizeof(*node));
    node->type = type;
    node->line = line;
    node->inferred = TY_UNKNOWN;
    node->ret_type = TY_VOID;
    return node;
}

void add_child(ASTNode *parent, ASTNode *child)
{
    if (!parent || !child)
        return;
    if (parent->child_count == parent->child_cap) {
        parent->child_cap = parent->child_cap ? parent->child_cap * 2 : 4;
        parent->children = redb_xrealloc(parent->children,
                                         parent->child_cap * sizeof(*parent->children));
    }
    parent->children[parent->child_count++] = child;
}

void free_ast(ASTNode *node)
{
    size_t i;

    if (!node)
        return;
    for (i = 0; i < node->child_count; i++)
        free_ast(node->children[i]);
    free(node->children);
    free(node);
}

static Token *peek(int offset)
{
    size_t at = p_pos + (size_t)offset;

    if (at >= p_count)
        return &p_tokens[p_count - 1]; /* EOF */
    return &p_tokens[at];
}

static Token *current(void)
{
    return peek(0);
}

static Token *advance(void)
{
    Token *tok = current();

    if (p_pos + 1 < p_count)
        p_pos++;
    return tok;
}

static int check(TokenType type)
{
    return current()->type == type;
}

static int match(TokenType type)
{
    if (check(type)) {
        advance();
        return 1;
    }
    return 0;
}

static void parse_fail(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "redb: error:%d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static Token *expect(TokenType type, const char *what)
{
    if (!check(type)) {
        parse_fail(current()->line, "expected %s, got '%s'",
                   what, current()->lexeme[0] ? current()->lexeme : "end of file");
    }
    return advance();
}

static RedbType parse_type(void)
{
    if (match(TOKEN_TYPE_INT))
        return TY_INT;
    if (match(TOKEN_TYPE_FLOAT))
        return TY_FLOAT;
    if (match(TOKEN_TYPE_DOUBLE))
        return TY_DOUBLE;
    if (match(TOKEN_TYPE_STR))
        return TY_STR;
    if (match(TOKEN_TYPE_VOID))
        return TY_VOID;
    if (check(TOKEN_LBRACE) && peek(1)->type == TOKEN_RBRACE) {
        advance();
        advance();
        return TY_VOID;
    }
    parse_fail(current()->line, "expected a type (int, float, double, str, void or {})");
    return TY_UNKNOWN;
}

static ASTNode *make_binary(const char *op, ASTNode *left, ASTNode *right, int line)
{
    ASTNode *node = create_node(AST_BINARY, line);

    snprintf(node->string_val, sizeof(node->string_val), "%s", op);
    add_child(node, left);
    add_child(node, right);
    return node;
}

static ASTNode *parse_primary(void)
{
    Token *tok = current();

    if (match(TOKEN_INT_LIT)) {
        ASTNode *node = create_node(AST_INT_LIT, tok->line);
        node->int_val = strtoll(tok->lexeme, NULL, 10);
        return node;
    }

    if (match(TOKEN_FLOAT_LIT)) {
        ASTNode *node = create_node(AST_FLOAT_LIT, tok->line);
        node->float_val = strtod(tok->lexeme, NULL);
        return node;
    }

    if (match(TOKEN_STRING_LIT)) {
        ASTNode *node = create_node(AST_STRING_LIT, tok->line);
        snprintf(node->string_val, sizeof(node->string_val), "%s", tok->lexeme);
        return node;
    }

    if (match(TOKEN_TRUE)) {
        ASTNode *node = create_node(AST_BOOL_LIT, tok->line);
        node->bool_val = 1;
        return node;
    }

    if (match(TOKEN_FALSE)) {
        ASTNode *node = create_node(AST_BOOL_LIT, tok->line);
        node->bool_val = 0;
        return node;
    }

    if (match(TOKEN_WRITE) || match(TOKEN_WRITELN)) {
        ASTNodeType type = tok->type == TOKEN_WRITE ? AST_CALL_WRITE : AST_CALL_WRITELN;
        ASTNode *node = create_node(type, tok->line);

        expect(TOKEN_LPAREN, "'(' after output function");
        add_child(node, parse_expression());
        expect(TOKEN_RPAREN, "')' after output argument");
        return node;
    }

    if (match(TOKEN_WRITEFORMAT)) {
        ASTNode *node = create_node(AST_CALL_FORMAT, tok->line);
        Token *str;

        expect(TOKEN_LPAREN, "'(' after writeformat");
        str = expect(TOKEN_STRING_LIT, "a string literal in writeformat");
        snprintf(node->string_val, sizeof(node->string_val), "%s", str->lexeme);
        expect(TOKEN_RPAREN, "')' after writeformat template");
        return node;
    }

    if (match(TOKEN_IDENTIFIER)) {
        ASTNode *node;

        if (match(TOKEN_LPAREN)) {
            node = create_node(AST_CALL, tok->line);
            snprintf(node->name, sizeof(node->name), "%s", tok->lexeme);
            if (!check(TOKEN_RPAREN)) {
                add_child(node, parse_expression());
                while (match(TOKEN_COMMA))
                    add_child(node, parse_expression());
            }
            expect(TOKEN_RPAREN, "')' after function arguments");
            return node;
        }

        node = create_node(AST_IDENT, tok->line);
        snprintf(node->name, sizeof(node->name), "%s", tok->lexeme);
        return node;
    }

    if (match(TOKEN_LPAREN)) {
        ASTNode *node = parse_expression();
        expect(TOKEN_RPAREN, "')' after expression");
        return node;
    }

    parse_fail(tok->line, "expected an expression, got '%s'",
               tok->lexeme[0] ? tok->lexeme : "end of file");
    return NULL;
}

static ASTNode *parse_unary(void)
{
    Token *tok = current();

    if (match(TOKEN_MINUS) || match(TOKEN_NOT)) {
        ASTNode *node = create_node(AST_UNARY, tok->line);

        snprintf(node->string_val, sizeof(node->string_val), "%s", tok->lexeme);
        add_child(node, parse_unary());
        return node;
    }
    return parse_primary();
}

static ASTNode *parse_multiplicative(void)
{
    ASTNode *left = parse_unary();

    while (check(TOKEN_STAR) || check(TOKEN_SLASH) || check(TOKEN_PERCENT)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_unary(), op->line);
    }
    return left;
}

static ASTNode *parse_additive(void)
{
    ASTNode *left = parse_multiplicative();

    while (check(TOKEN_PLUS) || check(TOKEN_MINUS)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_multiplicative(), op->line);
    }
    return left;
}

static ASTNode *parse_comparison(void)
{
    ASTNode *left = parse_additive();

    while (check(TOKEN_LT) || check(TOKEN_LE) || check(TOKEN_GT) || check(TOKEN_GE)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_additive(), op->line);
    }
    return left;
}

static ASTNode *parse_equality(void)
{
    ASTNode *left = parse_comparison();

    while (check(TOKEN_EQ) || check(TOKEN_NE)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_comparison(), op->line);
    }
    return left;
}

static ASTNode *parse_and(void)
{
    ASTNode *left = parse_equality();

    while (check(TOKEN_AND)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_equality(), op->line);
    }
    return left;
}

static ASTNode *parse_or(void)
{
    ASTNode *left = parse_and();

    while (check(TOKEN_OR)) {
        Token *op = advance();
        left = make_binary(op->lexeme, left, parse_and(), op->line);
    }
    return left;
}

static ASTNode *parse_expression(void)
{
    return parse_or();
}

static ASTNode *parse_function_expr(const char *name, int line)
{
    ASTNode *node = create_node(AST_FUNC_DECL, line);

    snprintf(node->name, sizeof(node->name), "%s", name);
    advance(); /* do / doto */
    expect(TOKEN_LPAREN, "'(' after function keyword");

    if (check(TOKEN_RPAREN)) {
        /* do() == do({} -> {}) */
    } else if (check(TOKEN_LBRACE) && peek(1)->type == TOKEN_RBRACE) {
        advance();
        advance();
    } else if (!check(TOKEN_ARROW) && !check(TOKEN_FAT_ARROW)) {
        if (node->param_count >= REDB_MAX_PARAMS)
            parse_fail(line, "too many parameters (max %d)", REDB_MAX_PARAMS);
        node->param_types[node->param_count++] = parse_type();
        while (match(TOKEN_COMMA)) {
            if (node->param_count >= REDB_MAX_PARAMS)
                parse_fail(line, "too many parameters (max %d)", REDB_MAX_PARAMS);
            node->param_types[node->param_count++] = parse_type();
        }
    }

    if (match(TOKEN_ARROW) || match(TOKEN_FAT_ARROW))
        node->ret_type = parse_type();
    else
        node->ret_type = TY_VOID;

    expect(TOKEN_RPAREN, "')' after function signature");
    add_child(node, parse_block());
    return node;
}

static ASTNode *parse_if_statement(void)
{
    Token *tok = expect(TOKEN_IF, "'if'");
    ASTNode *node = create_node(AST_IF, tok->line);

    expect(TOKEN_LPAREN, "'(' after if");
    add_child(node, parse_expression());
    expect(TOKEN_RPAREN, "')' after if condition");
    add_child(node, parse_block());

    if (match(TOKEN_ELSE)) {
        if (check(TOKEN_IF))
            add_child(node, parse_if_statement());
        else
            add_child(node, parse_block());
    }
    return node;
}

static ASTNode *parse_statement(void)
{
    Token *tok = current();

    if (match(TOKEN_SEMI))
        return NULL;

    if (check(TOKEN_FOR)) {
        ASTNode *node = create_node(AST_FOR_LOOP, tok->line);
        Token *var;

        advance();
        expect(TOKEN_LPAREN, "'(' after for");
        var = expect(TOKEN_IDENTIFIER, "a loop variable");
        set_name(node, var->lexeme);
        expect(TOKEN_IN, "'in' in for loop");
        add_child(node, parse_expression());
        expect(TOKEN_TO, "'to' in for loop");
        add_child(node, parse_expression());
        expect(TOKEN_RPAREN, "')' after for range");
        add_child(node, parse_block());
        return node;
    }

    if (check(TOKEN_WHILE)) {
        ASTNode *node = create_node(AST_WHILE_LOOP, tok->line);

        advance();
        expect(TOKEN_LPAREN, "'(' after while");
        add_child(node, parse_expression());
        expect(TOKEN_RPAREN, "')' after while condition");
        add_child(node, parse_block());
        return node;
    }

    if (check(TOKEN_IF))
        return parse_if_statement();

    if (check(TOKEN_RETURN)) {
        ASTNode *node = create_node(AST_RETURN, tok->line);

        advance();
        if (!check(TOKEN_RBRACE) && !check(TOKEN_SEMI) && !check(TOKEN_EOF) &&
            current()->line == tok->line)
            add_child(node, parse_expression());
        return node;
    }

    if (check(TOKEN_BREAK)) {
        advance();
        return create_node(AST_BREAK, tok->line);
    }

    if (check(TOKEN_CONTINUE)) {
        advance();
        return create_node(AST_CONTINUE, tok->line);
    }

    if (check(TOKEN_LBRACE))
        return parse_block();

    if (check(TOKEN_IDENTIFIER) && peek(1)->type == TOKEN_ASSIGN) {
        Token *name = advance();
        ASTNode *node;

        advance(); /* '=' */
        if (check(TOKEN_DO) || check(TOKEN_DOTO))
            return parse_function_expr(name->lexeme, name->line);

        node = create_node(AST_VAR_DECL, name->line);
        snprintf(node->name, sizeof(node->name), "%s", name->lexeme);
        add_child(node, parse_expression());
        return node;
    }

    return parse_expression();
}

static ASTNode *parse_block(void)
{
    Token *tok = expect(TOKEN_LBRACE, "'{'");
    ASTNode *block = create_node(AST_BLOCK, tok->line);

    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        ASTNode *stmt = parse_statement();

        if (stmt)
            add_child(block, stmt);
    }
    expect(TOKEN_RBRACE, "'}'");
    return block;
}

static ASTNode *parse_include(void)
{
    Token *tok = expect(TOKEN_AT, "'@'");
    ASTNode *node = create_node(AST_INCLUDE, tok->line);
    Token *path;

    expect(TOKEN_INCLUDE, "'include' after '@'");
    path = expect(TOKEN_STRING_LIT, "a quoted library name");
    snprintf(node->string_val, sizeof(node->string_val), "%s", path->lexeme);
    return node;
}

ASTNode *redb_parse(const char *filename)
{
    ASTNode *program = create_node(AST_PROGRAM, 1);

    p_tokens = g_tokens;
    p_count = g_token_count;
    p_pos = 0;
    p_file = filename;

    while (!check(TOKEN_EOF)) {
        if (match(TOKEN_SEMI))
            continue;

        if (check(TOKEN_AT)) {
            add_child(program, parse_include());
            continue;
        }

        if (check(TOKEN_MAIN)) {
            Token *tok = advance();
            ASTNode *fn = create_node(AST_FUNC_DECL, tok->line);

            snprintf(fn->name, sizeof(fn->name), "main");
            fn->is_main = 1;
            fn->ret_type = TY_INT;
            expect(TOKEN_LPAREN, "'(' after main");
            expect(TOKEN_RPAREN, "')' after main");
            add_child(fn, parse_block());
            add_child(program, fn);
            continue;
        }

        if (check(TOKEN_IDENTIFIER) && peek(1)->type == TOKEN_ASSIGN &&
            (peek(2)->type == TOKEN_DO || peek(2)->type == TOKEN_DOTO)) {
            Token *name = advance();

            advance(); /* '=' */
            add_child(program, parse_function_expr(name->lexeme, name->line));
            continue;
        }

        parse_fail(current()->line,
                   "only main(), function definitions and @include are allowed at top level");
    }

    (void)p_file;
    return program;
}

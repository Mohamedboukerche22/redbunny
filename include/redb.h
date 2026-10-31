/*
 * RedBunny (.redb) compiler core definitions.
 *
 * The compiler is a straight-line pipeline:
 *   lexer -> parser -> typecheck -> LLVM codegen
 */
#ifndef REDB_H
#define REDB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>

#define REDB_VERSION "0.1.0"
#define REDB_MAX_PARAMS 16

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_MAIN,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_TO,
    TOKEN_DOWNTO,
    TOKEN_WRITE,
    TOKEN_WRITELN,
    TOKEN_WRITEFORMAT,
    TOKEN_INCLUDE,
    TOKEN_DO,
    TOKEN_DOTO,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STR,
    TOKEN_TYPE_VOID,
    TOKEN_IDENTIFIER,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_ARROW,
    TOKEN_FAT_ARROW,
    TOKEN_COMMA,
    TOKEN_SEMI,
    TOKEN_AT
} TokenType;

#define REDB_LEXEME_MAX 2048

typedef struct {
    TokenType type;
    char lexeme[REDB_LEXEME_MAX];
    int line;
} Token;

typedef enum {
    TY_UNKNOWN = 0,
    TY_VOID,
    TY_INT,
    TY_FLOAT,
    TY_DOUBLE,
    TY_STR,
    TY_BOOL,
    TY_FUNC
} RedbType;

typedef enum {
    AST_PROGRAM = 0,
    AST_BLOCK,
    AST_INCLUDE,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_FOR_LOOP,
    AST_WHILE_LOOP,
    AST_IF,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_FUNC_DECL,
    AST_CALL_WRITE,
    AST_CALL_WRITELN,
    AST_CALL_FORMAT,
    AST_CALL,
    AST_BINARY,
    AST_UNARY,
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_STRING_LIT,
    AST_BOOL_LIT,
    AST_IDENT
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char name[128];
    char string_val[REDB_LEXEME_MAX];
    long long int_val;
    double float_val;
    int bool_val;
    int line;

    /* Type checker annotations. */
    RedbType inferred;
    RedbType ret_type;
    int param_count;
    RedbType param_types[REDB_MAX_PARAMS];

    int is_main;
    int descending;

    struct ASTNode **children;
    size_t child_count;
    size_t child_cap;
} ASTNode;

/* redb.c helpers */
void *redb_xmalloc(size_t size);
void *redb_xrealloc(void *ptr, size_t size);
char *redb_xstrdup(const char *s);
void redb_error(int line, const char *fmt, ...);
const char *redb_type_name(RedbType type);
RedbType redb_parse_type_name(const char *name);
char *redb_read_file(const char *path);
int redb_write_file(const char *path, const char *data);

/* lexer.c */
extern Token *g_tokens;
extern size_t g_token_count;
extern size_t g_token_cap;
extern int g_lex_error;
int redb_lex(const char *source, const char *filename);

/* parser.c */
ASTNode *create_node(ASTNodeType type, int line);
void add_child(ASTNode *parent, ASTNode *child);
void free_ast(ASTNode *node);
ASTNode *redb_parse(const char *filename);

/* include.c */
void redb_set_stdlib_dir(const char *dir);
int redb_expand_includes(ASTNode *program, const char *main_file);

/* typecheck.c */
void redb_typecheck(ASTNode *ast);

/* codegen.c */
int redb_codegen(ASTNode *ast, const char *ir_path);

#endif

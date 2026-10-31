#include "redb.h"

Token *g_tokens;
size_t g_token_count;
size_t g_token_cap;
int g_lex_error;

static const char *lex_file;
static int lex_line;

typedef struct {
    const char *word;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    { "main",        TOKEN_MAIN },
    { "for",         TOKEN_FOR },
    { "in",          TOKEN_IN },
    { "to",          TOKEN_TO },
    { "downTo",      TOKEN_DOWNTO },
    { "write",       TOKEN_WRITE },
    { "writeln",     TOKEN_WRITELN },
    { "writeformat", TOKEN_WRITEFORMAT },
    { "include",     TOKEN_INCLUDE },
    { "do",          TOKEN_DO },
    { "doto",        TOKEN_DOTO },
    { "return",      TOKEN_RETURN },
    { "if",          TOKEN_IF },
    { "else",        TOKEN_ELSE },
    { "while",       TOKEN_WHILE },
    { "true",        TOKEN_TRUE },
    { "false",       TOKEN_FALSE },
    { "break",       TOKEN_BREAK },
    { "continue",    TOKEN_CONTINUE },
    { "int",         TOKEN_TYPE_INT },
    { "float",       TOKEN_TYPE_FLOAT },
    { "double",      TOKEN_TYPE_DOUBLE },
    { "str",         TOKEN_TYPE_STR },
    { "void",        TOKEN_TYPE_VOID },
};

static void push_token(TokenType type, const char *lexeme, int line)
{
    Token *tok;

    if (g_token_count == g_token_cap) {
        g_token_cap = g_token_cap ? g_token_cap * 2 : 256;
        g_tokens = redb_xrealloc(g_tokens, g_token_cap * sizeof(*g_tokens));
    }

    tok = &g_tokens[g_token_count++];
    tok->type = type;
    tok->line = line;
    snprintf(tok->lexeme, sizeof(tok->lexeme), "%s", lexeme ? lexeme : "");
}

static TokenType keyword_type(const char *word)
{
    size_t i;

    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (!strcmp(word, keywords[i].word))
            return keywords[i].type;
    }
    return TOKEN_IDENTIFIER;
}

static int is_ident_start(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(int c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static void lex_string(const char *src, size_t *pos, char *out, size_t out_cap)
{
    size_t i = *pos;
    size_t o = 0;

    i++; /* opening quote */
    while (src[i] && src[i] != '"') {
        char c = src[i++];

        if (c == '\\') {
            char e = src[i++];
            switch (e) {
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            case '0':  c = '\0'; break;
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '{':  c = '{';  break;
            case '}':  c = '}';  break;
            default:
                redb_error(lex_line, "unknown escape sequence '\\%c'", e);
                g_lex_error = 1;
                c = e;
                break;
            }
        } else if (c == '\n') {
            lex_line++;
        }

        if (o + 1 < out_cap)
            out[o++] = c;
    }

    if (src[i] != '"') {
        redb_error(lex_line, "unterminated string literal");
        g_lex_error = 1;
    } else {
        i++;
    }

    out[o] = '\0';
    *pos = i;
}

int redb_lex(const char *source, const char *filename)
{
    size_t i = 0;

    g_token_count = 0;
    g_lex_error = 0;
    lex_file = filename;
    lex_line = 1;

    while (source[i]) {
        char c = source[i];

        if (c == '\n') {
            lex_line++;
            i++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            i++;
            continue;
        }

        /* -- single line comment */
        if (c == '-' && source[i + 1] == '-') {
            i += 2;
            while (source[i] && source[i] != '\n')
                i++;
            continue;
        }

        /* -* multi line comment *- */
        if (c == '-' && source[i + 1] == '*') {
            int start_line = lex_line;

            i += 2;
            while (source[i] && !(source[i] == '*' && source[i + 1] == '-')) {
                if (source[i] == '\n')
                    lex_line++;
                i++;
            }
            if (source[i]) {
                i += 2;
            } else {
                redb_error(start_line, "unterminated multi-line comment");
                g_lex_error = 1;
            }
            continue;
        }

        if (is_ident_start((unsigned char)c)) {
            char word[REDB_LEXEME_MAX];
            size_t start = i;
            size_t len;

            while (is_ident_char((unsigned char)source[i]))
                i++;
            len = i - start;
            if (len >= sizeof(word))
                len = sizeof(word) - 1;
            memcpy(word, source + start, len);
            word[len] = '\0';
            push_token(keyword_type(word), word, lex_line);
            continue;
        }

        if (c >= '0' && c <= '9') {
            char number[REDB_LEXEME_MAX];
            size_t start = i;
            size_t len;
            int is_float = 0;

            while (source[i] >= '0' && source[i] <= '9')
                i++;
            if (source[i] == '.' && source[i + 1] >= '0' && source[i + 1] <= '9') {
                is_float = 1;
                i++;
                while (source[i] >= '0' && source[i] <= '9')
                    i++;
            }
            if (source[i] == 'e' || source[i] == 'E') {
                size_t save = i;
                i++;
                if (source[i] == '+' || source[i] == '-')
                    i++;
                if (source[i] >= '0' && source[i] <= '9') {
                    is_float = 1;
                    while (source[i] >= '0' && source[i] <= '9')
                        i++;
                } else {
                    i = save;
                }
            }

            len = i - start;
            if (len >= sizeof(number))
                len = sizeof(number) - 1;
            memcpy(number, source + start, len);
            number[len] = '\0';
            push_token(is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, number, lex_line);
            continue;
        }

        if (c == '"') {
            char str[REDB_LEXEME_MAX];
            size_t start_line = lex_line;

            lex_string(source, &i, str, sizeof(str));
            push_token(TOKEN_STRING_LIT, str, (int)start_line);
            continue;
        }

        /* Multi character operators. */
        if (c == '-' && source[i + 1] == '>') {
            push_token(TOKEN_ARROW, "->", lex_line);
            i += 2;
            continue;
        }
        if (c == '=' && source[i + 1] == '>') {
            push_token(TOKEN_FAT_ARROW, "=>", lex_line);
            i += 2;
            continue;
        }
        if (c == '=' && source[i + 1] == '=') {
            push_token(TOKEN_EQ, "==", lex_line);
            i += 2;
            continue;
        }
        if (c == '!' && source[i + 1] == '=') {
            push_token(TOKEN_NE, "!=", lex_line);
            i += 2;
            continue;
        }
        if (c == '<' && source[i + 1] == '=') {
            push_token(TOKEN_LE, "<=", lex_line);
            i += 2;
            continue;
        }
        if (c == '>' && source[i + 1] == '=') {
            push_token(TOKEN_GE, ">=", lex_line);
            i += 2;
            continue;
        }
        if (c == '&' && source[i + 1] == '&') {
            push_token(TOKEN_AND, "&&", lex_line);
            i += 2;
            continue;
        }
        if (c == '|' && source[i + 1] == '|') {
            push_token(TOKEN_OR, "||", lex_line);
            i += 2;
            continue;
        }

        switch (c) {
        case '=': push_token(TOKEN_ASSIGN, "=", lex_line); break;
        case '+': push_token(TOKEN_PLUS, "+", lex_line); break;
        case '-': push_token(TOKEN_MINUS, "-", lex_line); break;
        case '*': push_token(TOKEN_STAR, "*", lex_line); break;
        case '/': push_token(TOKEN_SLASH, "/", lex_line); break;
        case '%': push_token(TOKEN_PERCENT, "%", lex_line); break;
        case '<': push_token(TOKEN_LT, "<", lex_line); break;
        case '>': push_token(TOKEN_GT, ">", lex_line); break;
        case '!': push_token(TOKEN_NOT, "!", lex_line); break;
        case '(': push_token(TOKEN_LPAREN, "(", lex_line); break;
        case ')': push_token(TOKEN_RPAREN, ")", lex_line); break;
        case '{': push_token(TOKEN_LBRACE, "{", lex_line); break;
        case '}': push_token(TOKEN_RBRACE, "}", lex_line); break;
        case ',': push_token(TOKEN_COMMA, ",", lex_line); break;
        case ';': push_token(TOKEN_SEMI, ";", lex_line); break;
        case '@': push_token(TOKEN_AT, "@", lex_line); break;
        default:
            redb_error(lex_line, "unexpected character '%c' (0x%02x)", c, (unsigned char)c);
            g_lex_error = 1;
            break;
        }
        i++;
    }

    push_token(TOKEN_EOF, "", lex_line);
    return g_lex_error ? -1 : 0;
}

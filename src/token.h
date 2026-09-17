#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INVALID,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,
    TOKEN_REAL_LITERAL,
    TOKEN_RECEBA,
    TOKEN_REPETE,
    TOKEN_DURANTE,
    TOKEN_COND,
    TOKEN_CASOCONTRARIO,
    TOKEN_FUNC,
    TOKEN_BOOL,
    TOKEN_INTEIRA,
    TOKEN_FLUT,
    TOKEN_PALAVRA,
    TOKEN_DUPLOCARPADO,
    TOKEN_PRINCIPAL,
    TOKEN_VAZIO,
    TOKEN_RESPOST,
    TOKEN_REGISTRO,
    TOKEN_OUTRAFUNCAO,
    TOKEN_EDAI,
    TOKEN_VET,
    TOKEN_VER,
    TOKEN_FAL,
    TOKEN_OU,
    TOKEN_E,
    TOKEN_NAO,
    TOKEN_XOR,
    TOKEN_RAIZ,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_INTEGER_DIV,
    TOKEN_CARET,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_AT
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

void token_list_init(TokenList *list);
void token_list_free(TokenList *list);
int token_list_append(TokenList *list, TokenType type, const char *start,
                      size_t length, int line, int column);
size_t token_list_count_type(const TokenList *list, TokenType type);
const char *token_type_name(TokenType type);
void token_list_print(const TokenList *list, FILE *stream);

#endif

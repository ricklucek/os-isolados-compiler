#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    const char *lexeme;
    int line;
    int column;
} Token;

void token_module_placeholder(void);

#endif

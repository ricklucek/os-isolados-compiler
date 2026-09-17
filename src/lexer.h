#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "token.h"

typedef enum {
    LEXER_OK = 0,
    LEXER_HAS_ERRORS = 1,
    LEXER_IO_ERROR = 2,
    LEXER_MEMORY_ERROR = 3
} LexerStatus;

/*
 * Converte o arquivo fonte em uma lista de tokens.
 * Ao encontrar caracteres inválidos, registra TOKEN_INVALID, reporta o erro
 * e continua a varredura para permitir múltiplos diagnósticos.
 */
LexerStatus lexer_scan_file(const char *path, TokenList *tokens, FILE *error_stream);

#endif

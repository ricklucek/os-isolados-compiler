#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <stdio.h>

#include "token.h"

typedef enum {
    PARSER_OK = 0,
    PARSER_HAS_ERRORS = 1
} ParserStatus;

typedef struct {
    ParserStatus status;
    size_t error_count;
} ParserResult;

/* Valida sintaticamente a lista de tokens produzida pelo lexer. */
ParserResult parser_parse(const TokenList *tokens, FILE *error_stream);

#endif

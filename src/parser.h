#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <stdio.h>

#include "ast.h"
#include "token.h"

typedef enum {
    PARSER_OK = 0,
    PARSER_HAS_ERRORS = 1,
    PARSER_MEMORY_ERROR = 2
} ParserStatus;

typedef struct {
    ParserStatus status;
    size_t error_count;
} ParserResult;

/* Valida sintaticamente a lista de tokens e descarta a AST gerada. */
ParserResult parser_parse(const TokenList *tokens, FILE *error_stream);

/*
 * Valida a lista de tokens e, quando não há erros, devolve a AST em out_ast.
 * Em caso de erro sintático ou de memória, out_ast recebe NULL.
 */
ParserResult parser_parse_ast(const TokenList *tokens, AstNode **out_ast,
                              FILE *error_stream);

#endif

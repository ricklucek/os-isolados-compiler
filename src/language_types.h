#ifndef LANGUAGE_TYPES_H
#define LANGUAGE_TYPES_H

#include "token.h"

/*
 * Tipos semanticos da Macaronica.
 *
 * Este modulo e independente da tabela de simbolos para que AST, analisador
 * semantico e futuros back-ends compartilhem o mesmo contrato de tipos.
 */
typedef enum {
    LANG_TYPE_ERROR = 0,
    LANG_TYPE_BOOL,
    LANG_TYPE_INTEIRA,
    LANG_TYPE_FLUT,
    LANG_TYPE_PALAVRA,
    LANG_TYPE_DUPLOCARPADO,
    LANG_TYPE_RECORD,
    LANG_TYPE_VAZIO
} LangType;

LangType lang_type_from_token(TokenType token_type);
const char *lang_type_name(LangType type);
int lang_type_is_numeric(LangType type);
int lang_type_can_assign(LangType target, LangType source);
LangType lang_type_common_numeric(LangType left, LangType right);

#endif

#include "token.h"

#include <stdlib.h>
#include <string.h>

#define TOKEN_INITIAL_CAPACITY 32U

void token_list_init(TokenList *list) {
    if (list == NULL) return;
    list->items = NULL;
    list->count = 0U;
    list->capacity = 0U;
}

void token_list_free(TokenList *list) {
    size_t i;
    if (list == NULL) return;
    for (i = 0U; i < list->count; ++i) free(list->items[i].lexeme);
    free(list->items);
    token_list_init(list);
}

static int token_list_grow(TokenList *list) {
    size_t new_capacity = list->capacity == 0U ? TOKEN_INITIAL_CAPACITY : list->capacity * 2U;
    Token *new_items = realloc(list->items, new_capacity * sizeof(*new_items));
    if (new_items == NULL) return 0;
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

int token_list_append(TokenList *list, TokenType type, const char *start,
                      size_t length, int line, int column) {
    char *lexeme;
    if (list == NULL || (start == NULL && length != 0U)) return 0;
    if (list->count == list->capacity && !token_list_grow(list)) return 0;
    lexeme = malloc(length + 1U);
    if (lexeme == NULL) return 0;
    if (length > 0U) memcpy(lexeme, start, length);
    lexeme[length] = '\0';
    list->items[list->count].type = type;
    list->items[list->count].lexeme = lexeme;
    list->items[list->count].line = line;
    list->items[list->count].column = column;
    ++list->count;
    return 1;
}

size_t token_list_count_type(const TokenList *list, TokenType type) {
    size_t i, count = 0U;
    if (list == NULL) return 0U;
    for (i = 0U; i < list->count; ++i) if (list->items[i].type == type) ++count;
    return count;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_INVALID: return "INVALID";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TOKEN_REAL_LITERAL: return "REAL_LITERAL";
        case TOKEN_STRING_LITERAL: return "STRING_LITERAL";
        case TOKEN_RECEBA: return "RECEBA";
        case TOKEN_REPETE: return "REPETE";
        case TOKEN_DURANTE: return "DURANTE";
        case TOKEN_COND: return "COND";
        case TOKEN_CASOCONTRARIO: return "CASOCONTRARIO";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_INTEIRA: return "INTEIRA";
        case TOKEN_FLUT: return "FLUT";
        case TOKEN_PALAVRA: return "PALAVRA";
        case TOKEN_DUPLOCARPADO: return "DUPLOCARPADO";
        case TOKEN_PRINCIPAL: return "PRINCIPAL";
        case TOKEN_VAZIO: return "VAZIO";
        case TOKEN_RESPOST: return "RESPOST";
        case TOKEN_REGISTRO: return "REGISTRO";
        case TOKEN_OUTRAFUNCAO: return "OUTRAFUNCAO";
        case TOKEN_EDAI: return "EDAI";
        case TOKEN_VET: return "VET";
        case TOKEN_VER: return "VER";
        case TOKEN_FAL: return "FAL";
        case TOKEN_OU: return "OU";
        case TOKEN_E: return "E";
        case TOKEN_NAO: return "NAO";
        case TOKEN_XOR: return "XOR";
        case TOKEN_RAIZ: return "RAIZ";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_INTEGER_DIV: return "INTEGER_DIV";
        case TOKEN_CARET: return "CARET";
        case TOKEN_LESS: return "LESS";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TOKEN_BANG: return "BANG";
        case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_AT: return "AT";
        default: return "UNKNOWN";
    }
}

void token_list_print(const TokenList *list, FILE *stream) {
    size_t i;
    if (list == NULL || stream == NULL) return;
    fprintf(stream, "%-6s %-6s %-20s %s\n", "LINHA", "COL", "TOKEN", "LEXEMA");
    fprintf(stream, "%-6s %-6s %-20s %s\n", "-----", "---", "-----", "------");
    for (i = 0U; i < list->count; ++i) {
        const Token *token = &list->items[i];
        fprintf(stream, "%-6d %-6d %-20s %s\n", token->line, token->column,
                token_type_name(token->type), token->type == TOKEN_EOF ? "<EOF>" : token->lexeme);
    }
}

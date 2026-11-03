#include "language_types.h"

/*
 * Regras de tipos compartilhadas entre a analise semantica e os back-ends.
 * O gerador de codigo nao deve reinventar promocao ou compatibilidade.
 */

LangType lang_type_from_token(TokenType token_type) {
    switch (token_type) {
        case TOKEN_BOOL: return LANG_TYPE_BOOL;
        case TOKEN_INTEIRA: return LANG_TYPE_INTEIRA;
        case TOKEN_FLUT: return LANG_TYPE_FLUT;
        case TOKEN_PALAVRA: return LANG_TYPE_PALAVRA;
        case TOKEN_DUPLOCARPADO: return LANG_TYPE_DUPLOCARPADO;
        case TOKEN_VAZIO: return LANG_TYPE_VAZIO;
        default: return LANG_TYPE_ERROR;
    }
}

const char *lang_type_name(LangType type) {
    switch (type) {
        case LANG_TYPE_BOOL: return "bool";
        case LANG_TYPE_INTEIRA: return "inteira";
        case LANG_TYPE_FLUT: return "flut";
        case LANG_TYPE_PALAVRA: return "palavra";
        case LANG_TYPE_DUPLOCARPADO: return "duplocarpado";
        case LANG_TYPE_RECORD: return "registro";
        case LANG_TYPE_VAZIO: return "vazio";
        case LANG_TYPE_ERROR: return "<erro>";
        default: return "<tipo-desconhecido>";
    }
}

int lang_type_is_numeric(LangType type) {
    return type == LANG_TYPE_INTEIRA || type == LANG_TYPE_FLUT ||
           type == LANG_TYPE_DUPLOCARPADO;
}

static int numeric_rank(LangType type) {
    switch (type) {
        case LANG_TYPE_INTEIRA: return 1;
        case LANG_TYPE_FLUT: return 2;
        case LANG_TYPE_DUPLOCARPADO: return 3;
        default: return 0;
    }
}

int lang_type_can_assign(LangType target, LangType source) {
    if (target == LANG_TYPE_ERROR || source == LANG_TYPE_ERROR) return 0;
    if (target == source) return 1;
    if (lang_type_is_numeric(target) && lang_type_is_numeric(source))
        return numeric_rank(source) <= numeric_rank(target);
    return 0;
}

LangType lang_type_common_numeric(LangType left, LangType right) {
    if (!lang_type_is_numeric(left) || !lang_type_is_numeric(right))
        return LANG_TYPE_ERROR;
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

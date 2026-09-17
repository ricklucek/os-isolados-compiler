#include "parser.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    const TokenList *tokens;
    size_t current;
    size_t error_count;
    FILE *error_stream;
} Parser;

static int parse_top_level(Parser *parser);
static int parse_statement(Parser *parser);
static int parse_block(Parser *parser);
static int parse_expression(Parser *parser);
static int parse_condition(Parser *parser);
static int parse_assignment_core(Parser *parser);

static const Token *peek_token(const Parser *parser) {
    if (parser->tokens == NULL || parser->tokens->count == 0U) return NULL;
    if (parser->current >= parser->tokens->count)
        return &parser->tokens->items[parser->tokens->count - 1U];
    return &parser->tokens->items[parser->current];
}

static const Token *previous_token(const Parser *parser) {
    if (parser->tokens == NULL || parser->current == 0U) return NULL;
    return &parser->tokens->items[parser->current - 1U];
}

static int is_at_end(const Parser *parser) {
    const Token *token = peek_token(parser);
    return token == NULL || token->type == TOKEN_EOF;
}

static int check(const Parser *parser, TokenType type) {
    const Token *token = peek_token(parser);
    return token != NULL && token->type == type;
}

static const Token *advance_token(Parser *parser) {
    if (!is_at_end(parser)) ++parser->current;
    return previous_token(parser);
}

static int match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return 0;
    advance_token(parser);
    return 1;
}

static void parser_error_at(Parser *parser, const Token *token, const char *format, ...) {
    va_list args;
    FILE *stream = parser->error_stream != NULL ? parser->error_stream : stderr;

    ++parser->error_count;
    if (token == NULL) {
        fprintf(stream, "[ERRO SINTATICO] fim inesperado da entrada: ");
    } else if (token->type == TOKEN_EOF) {
        fprintf(stream, "[ERRO SINTATICO] linha %d, coluna %d, no fim do arquivo: ",
                token->line, token->column);
    } else {
        fprintf(stream, "[ERRO SINTATICO] linha %d, coluna %d, token '%s' (%s): ",
                token->line, token->column, token->lexeme, token_type_name(token->type));
    }

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fputc('\n', stream);
}

static int consume(Parser *parser, TokenType type, const char *message) {
    if (check(parser, type)) {
        advance_token(parser);
        return 1;
    }
    parser_error_at(parser, peek_token(parser), "%s; esperado %s.",
                    message, token_type_name(type));
    return 0;
}

static int is_value_type(TokenType type) {
    return type == TOKEN_BOOL || type == TOKEN_INTEIRA || type == TOKEN_FLUT ||
           type == TOKEN_PALAVRA || type == TOKEN_DUPLOCARPADO;
}

static int is_return_type(TokenType type) {
    return is_value_type(type) || type == TOKEN_VAZIO;
}

static int starts_statement(TokenType type) {
    return is_value_type(type) || type == TOKEN_IDENTIFIER || type == TOKEN_RESPOST ||
           type == TOKEN_COND || type == TOKEN_DURANTE || type == TOKEN_REPETE;
}

static int starts_top_level(TokenType type) {
    return type == TOKEN_REGISTRO || type == TOKEN_OUTRAFUNCAO || is_return_type(type);
}

static void synchronize_statement(Parser *parser) {
    while (!is_at_end(parser)) {
        const Token *previous = previous_token(parser);
        const Token *current = peek_token(parser);

        if (previous != NULL && previous->type == TOKEN_SEMICOLON) return;
        if (current != NULL && current->type == TOKEN_RIGHT_BRACKET) return;
        if (current != NULL && starts_statement(current->type)) return;
        advance_token(parser);
    }
}

static void synchronize_top_level(Parser *parser) {
    while (!is_at_end(parser)) {
        const Token *current = peek_token(parser);
        if (current != NULL && starts_top_level(current->type)) return;
        advance_token(parser);
    }
}

static void recover_until(Parser *parser, TokenType type) {
    while (!is_at_end(parser) && !check(parser, type)) advance_token(parser);
}

static int parse_value_type(Parser *parser, const char *context) {
    const Token *token = peek_token(parser);
    if (token != NULL && is_value_type(token->type)) {
        advance_token(parser);
        return 1;
    }
    parser_error_at(parser, token,
                    "%s exige um tipo de valor (bool, inteira, flut, palavra ou duplocarpado)",
                    context);
    return 0;
}

static int parse_return_type(Parser *parser, const char *context) {
    const Token *token = peek_token(parser);
    if (token != NULL && is_return_type(token->type)) {
        advance_token(parser);
        return 1;
    }
    parser_error_at(parser, token, "%s exige um tipo de retorno valido", context);
    return 0;
}

static int parse_vector_suffix(Parser *parser) {
    int ok = 1;

    if (!match(parser, TOKEN_VET)) return 1;
    if (!consume(parser, TOKEN_LEFT_BRACE,
                 "declaracao de vetor exige '{' depois de vet"))
        return 0;
    if (!match(parser, TOKEN_INTEGER_LITERAL)) {
        parser_error_at(parser, peek_token(parser),
                        "tamanho do vetor deve ser um literal inteiro; esperado %s",
                        token_type_name(TOKEN_INTEGER_LITERAL));
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_BRACE);
    }
    if (!consume(parser, TOKEN_RIGHT_BRACE,
                 "declaracao de vetor exige '}' depois do tamanho"))
        ok = 0;
    return ok;
}

static int parse_declarator_list(Parser *parser) {
    if (!consume(parser, TOKEN_IDENTIFIER, "declaracao exige um identificador")) return 0;
    while (match(parser, TOKEN_COMMA)) {
        if (!consume(parser, TOKEN_IDENTIFIER, "esperado identificador depois de ','")) return 0;
    }
    return consume(parser, TOKEN_SEMICOLON, "declaracao deve terminar com ';'");
}

static int parse_variable_declaration(Parser *parser) {
    int ok = 1;
    if (!parse_value_type(parser, "declaracao")) return 0;
    if (!parse_vector_suffix(parser)) ok = 0;
    if (!parse_declarator_list(parser)) ok = 0;
    return ok;
}

static int parse_parameter(Parser *parser) {
    if (!parse_value_type(parser, "parametro")) return 0;
    return consume(parser, TOKEN_IDENTIFIER, "parametro exige um nome");
}

static int parse_parameter_list(Parser *parser) {
    if (check(parser, TOKEN_RIGHT_PAREN)) return 1;
    if (!parse_parameter(parser)) return 0;
    while (match(parser, TOKEN_COMMA)) {
        if (!parse_parameter(parser)) return 0;
    }
    return 1;
}

static int parse_argument_list(Parser *parser) {
    if (check(parser, TOKEN_RIGHT_PAREN)) return 1;
    if (!parse_expression(parser)) return 0;
    while (match(parser, TOKEN_COMMA)) {
        if (!parse_expression(parser)) return 0;
    }
    return 1;
}

static int parse_primary(Parser *parser) {
    if (match(parser, TOKEN_INTEGER_LITERAL) || match(parser, TOKEN_REAL_LITERAL) ||
        match(parser, TOKEN_VER) || match(parser, TOKEN_FAL))
        return 1;

    if (match(parser, TOKEN_RAIZ)) {
        if (!consume(parser, TOKEN_LEFT_PAREN, "raiz exige '('") ||
            !parse_expression(parser) ||
            !consume(parser, TOKEN_RIGHT_PAREN, "raiz exige ')' depois do argumento"))
            return 0;
        return 1;
    }

    if (match(parser, TOKEN_IDENTIFIER)) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            if (!parse_argument_list(parser)) return 0;
            return consume(parser, TOKEN_RIGHT_PAREN, "chamada de funcao exige ')'");
        }
        if (match(parser, TOKEN_LEFT_BRACE)) {
            if (!parse_expression(parser)) return 0;
            return consume(parser, TOKEN_RIGHT_BRACE,
                           "acesso a vetor exige '}' depois do indice");
        }
        return 1;
    }

    if (match(parser, TOKEN_LEFT_PAREN)) {
        if (!parse_expression(parser)) return 0;
        return consume(parser, TOKEN_RIGHT_PAREN, "expressao agrupada exige ')'");
    }

    parser_error_at(parser, peek_token(parser), "esperada expressao");
    return 0;
}

static int parse_unary(Parser *parser) {
    if (match(parser, TOKEN_MINUS) || match(parser, TOKEN_PLUS) || match(parser, TOKEN_NAO))
        return parse_unary(parser);
    return parse_primary(parser);
}

static int parse_power(Parser *parser) {
    if (!parse_unary(parser)) return 0;
    if (match(parser, TOKEN_CARET)) return parse_power(parser);
    return 1;
}

static int parse_factor(Parser *parser) {
    if (!parse_power(parser)) return 0;
    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) ||
           match(parser, TOKEN_INTEGER_DIV)) {
        if (!parse_power(parser)) return 0;
    }
    return 1;
}

static int parse_term(Parser *parser) {
    if (!parse_factor(parser)) return 0;
    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        if (!parse_factor(parser)) return 0;
    }
    return 1;
}

static int parse_comparison_expression(Parser *parser) {
    if (!parse_term(parser)) return 0;
    while (match(parser, TOKEN_LESS) || match(parser, TOKEN_GREATER)) {
        if (!parse_term(parser)) return 0;
    }
    return 1;
}

static int parse_equality(Parser *parser) {
    if (!parse_comparison_expression(parser)) return 0;
    while (match(parser, TOKEN_EQUAL_EQUAL) || match(parser, TOKEN_BANG)) {
        if (!parse_comparison_expression(parser)) return 0;
    }
    return 1;
}

static int parse_logical_and(Parser *parser) {
    if (!parse_equality(parser)) return 0;
    while (match(parser, TOKEN_E)) {
        if (!parse_equality(parser)) return 0;
    }
    return 1;
}

static int parse_logical_xor(Parser *parser) {
    if (!parse_logical_and(parser)) return 0;
    while (match(parser, TOKEN_XOR)) {
        if (!parse_logical_and(parser)) return 0;
    }
    return 1;
}

static int parse_expression(Parser *parser) {
    if (!parse_logical_xor(parser)) return 0;
    while (match(parser, TOKEN_OU)) {
        if (!parse_logical_xor(parser)) return 0;
    }
    return 1;
}

static int parse_condition_operand(Parser *parser) {
    if (match(parser, TOKEN_INTEGER_LITERAL) || match(parser, TOKEN_REAL_LITERAL)) return 1;
    if (match(parser, TOKEN_IDENTIFIER)) {
        if (match(parser, TOKEN_LEFT_BRACE)) {
            if (!parse_expression(parser)) return 0;
            return consume(parser, TOKEN_RIGHT_BRACE,
                           "acesso a vetor na condicao exige '}'");
        }
        return 1;
    }
    parser_error_at(parser, peek_token(parser),
                    "condicao exige VAR ou NUM como operando, conforme a especificacao");
    return 0;
}

static int parse_comparator(Parser *parser) {
    if (match(parser, TOKEN_LESS) || match(parser, TOKEN_GREATER) ||
        match(parser, TOKEN_EQUAL_EQUAL) || match(parser, TOKEN_BANG))
        return 1;
    parser_error_at(parser, peek_token(parser),
                    "condicao exige comparador '<', '>', '==' ou '!'");
    return 0;
}

static int parse_condition(Parser *parser) {
    return parse_condition_operand(parser) && parse_comparator(parser) &&
           parse_condition_operand(parser);
}

static int parse_lvalue(Parser *parser) {
    if (!consume(parser, TOKEN_IDENTIFIER,
                 "atribuicao exige identificador no lado esquerdo"))
        return 0;
    if (match(parser, TOKEN_LEFT_BRACE)) {
        if (!parse_expression(parser)) return 0;
        if (!consume(parser, TOKEN_RIGHT_BRACE,
                     "acesso a vetor exige '}' depois do indice"))
            return 0;
    }
    return 1;
}

static int parse_assignment_core(Parser *parser) {
    if (!parse_lvalue(parser)) return 0;
    if (!consume(parser, TOKEN_RECEBA, "atribuicao exige a palavra 'receba'")) return 0;
    return parse_expression(parser);
}

static int parse_identifier_statement(Parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) return 0;

    /* Chamada de funcao usada como comando. */
    if (parser->current + 1U < parser->tokens->count &&
        parser->tokens->items[parser->current + 1U].type == TOKEN_LEFT_PAREN) {
        advance_token(parser);
        advance_token(parser);
        if (!parse_argument_list(parser)) return 0;
        if (!consume(parser, TOKEN_RIGHT_PAREN, "chamada de funcao exige ')'")) return 0;
        return consume(parser, TOKEN_SEMICOLON,
                       "chamada de funcao deve terminar com ';'");
    }

    if (!parse_assignment_core(parser)) return 0;
    return consume(parser, TOKEN_SEMICOLON, "atribuicao deve terminar com ';'");
}

static int parse_return_statement(Parser *parser) {
    if (!consume(parser, TOKEN_RESPOST, "esperado 'respost'")) return 0;
    if (match(parser, TOKEN_SEMICOLON)) return 1;
    if (!parse_expression(parser)) return 0;
    return consume(parser, TOKEN_SEMICOLON, "retorno deve terminar com ';'");
}

static int parse_conditional(Parser *parser) {
    int ok = 1;

    if (!consume(parser, TOKEN_COND, "esperado 'cond'")) return 0;
    if (!consume(parser, TOKEN_LEFT_PAREN, "cond exige '('") ) ok = 0;
    if (!parse_condition(parser)) {
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_PAREN);
    }
    if (!consume(parser, TOKEN_RIGHT_PAREN, "cond exige ')' depois da condicao")) ok = 0;
    if (!consume(parser, TOKEN_EDAI, "cond exige 'edai'")) ok = 0;
    if (!parse_block(parser)) ok = 0;

    while (match(parser, TOKEN_CASOCONTRARIO)) {
        if (match(parser, TOKEN_COND)) {
            if (!consume(parser, TOKEN_LEFT_PAREN,
                         "casocontrario cond exige '('") )
                ok = 0;
            if (!parse_condition(parser)) {
                ok = 0;
                recover_until(parser, TOKEN_RIGHT_PAREN);
            }
            if (!consume(parser, TOKEN_RIGHT_PAREN,
                         "casocontrario cond exige ')'") )
                ok = 0;
            if (!consume(parser, TOKEN_EDAI,
                         "casocontrario cond exige 'edai'"))
                ok = 0;
            if (!parse_block(parser)) ok = 0;
        } else {
            if (!consume(parser, TOKEN_EDAI, "casocontrario exige 'edai'")) ok = 0;
            if (!parse_block(parser)) ok = 0;
            break;
        }
    }
    return ok;
}

static int parse_while(Parser *parser) {
    int ok = 1;

    if (!consume(parser, TOKEN_DURANTE, "esperado 'durante'")) return 0;
    if (!consume(parser, TOKEN_LEFT_PAREN, "durante exige '('") ) ok = 0;
    if (!parse_condition(parser)) {
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_PAREN);
    }
    if (!consume(parser, TOKEN_RIGHT_PAREN,
                 "durante exige ')' depois da condicao"))
        ok = 0;
    if (!consume(parser, TOKEN_EDAI, "durante exige 'edai'")) ok = 0;
    if (!parse_block(parser)) ok = 0;
    return ok;
}

static int parse_for(Parser *parser) {
    int ok = 1;

    if (!consume(parser, TOKEN_REPETE, "esperado 'repete'")) return 0;
    if (!consume(parser, TOKEN_LEFT_PAREN, "repete exige '('") ) ok = 0;

    if (!parse_assignment_core(parser) ||
        !consume(parser, TOKEN_COMMA,
                 "repete separa inicializacao e condicao com ','") ||
        !parse_condition(parser) ||
        !consume(parser, TOKEN_COMMA,
                 "repete separa condicao e iteracao com ','") ||
        !parse_assignment_core(parser)) {
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_PAREN);
    }

    if (!consume(parser, TOKEN_RIGHT_PAREN,
                 "repete exige ')' depois da iteracao"))
        ok = 0;
    if (!consume(parser, TOKEN_EDAI, "repete exige 'edai'")) ok = 0;
    if (!parse_block(parser)) ok = 0;
    return ok;
}

static int parse_statement(Parser *parser) {
    const Token *token = peek_token(parser);
    if (token == NULL) return 0;

    if (is_value_type(token->type)) return parse_variable_declaration(parser);
    if (token->type == TOKEN_IDENTIFIER) return parse_identifier_statement(parser);
    if (token->type == TOKEN_RESPOST) return parse_return_statement(parser);
    if (token->type == TOKEN_COND) return parse_conditional(parser);
    if (token->type == TOKEN_DURANTE) return parse_while(parser);
    if (token->type == TOKEN_REPETE) return parse_for(parser);

    parser_error_at(parser, token, "token nao inicia um comando valido");
    return 0;
}

static int parse_block(Parser *parser) {
    if (!consume(parser, TOKEN_LEFT_BRACKET, "bloco exige '['")) return 0;

    while (!check(parser, TOKEN_RIGHT_BRACKET) && !is_at_end(parser)) {
        size_t before = parser->current;
        if (!parse_statement(parser)) {
            synchronize_statement(parser);
            if (parser->current == before && !is_at_end(parser) &&
                !check(parser, TOKEN_RIGHT_BRACKET))
                advance_token(parser);
        }
    }

    return consume(parser, TOKEN_RIGHT_BRACKET, "bloco exige ']'");
}

static int parse_record(Parser *parser) {
    if (!consume(parser, TOKEN_REGISTRO, "esperado 'registro'") ||
        !consume(parser, TOKEN_IDENTIFIER, "registro exige um nome") ||
        !consume(parser, TOKEN_AT, "registro exige '@'") ||
        !consume(parser, TOKEN_LEFT_BRACKET, "registro exige '['"))
        return 0;

    while (!check(parser, TOKEN_RIGHT_BRACKET) && !is_at_end(parser)) {
        size_t before = parser->current;
        if (!parse_variable_declaration(parser)) {
            synchronize_statement(parser);
            if (parser->current == before && !is_at_end(parser) &&
                !check(parser, TOKEN_RIGHT_BRACKET))
                advance_token(parser);
        }
    }
    return consume(parser, TOKEN_RIGHT_BRACKET, "registro exige ']'");
}

static int parse_function(Parser *parser) {
    int ok = 1;

    if (!consume(parser, TOKEN_OUTRAFUNCAO, "esperado 'outrafuncao'")) return 0;
    if (!parse_return_type(parser, "outrafuncao")) ok = 0;
    if (!consume(parser, TOKEN_IDENTIFIER, "funcao exige um nome")) ok = 0;
    if (!consume(parser, TOKEN_LEFT_PAREN, "funcao exige '('") ) ok = 0;
    if (!parse_parameter_list(parser)) {
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_PAREN);
    }
    if (!consume(parser, TOKEN_RIGHT_PAREN,
                 "funcao exige ')' depois dos parametros"))
        ok = 0;
    if (!consume(parser, TOKEN_AT, "funcao exige '@' antes do bloco")) ok = 0;
    if (!parse_block(parser)) ok = 0;
    return ok;
}

static int parse_principal(Parser *parser) {
    int ok = 1;

    if (!parse_return_type(parser, "principal")) ok = 0;
    if (!consume(parser, TOKEN_PRINCIPAL, "esperado 'principal'")) ok = 0;
    if (!consume(parser, TOKEN_LEFT_PAREN, "principal exige '('") ) ok = 0;
    if (!parse_parameter_list(parser)) {
        ok = 0;
        recover_until(parser, TOKEN_RIGHT_PAREN);
    }
    if (!consume(parser, TOKEN_RIGHT_PAREN,
                 "principal exige ')' depois dos parametros"))
        ok = 0;
    if (!consume(parser, TOKEN_AT, "principal exige '@' antes do bloco")) ok = 0;
    if (!parse_block(parser)) ok = 0;
    return ok;
}

static int parse_top_level(Parser *parser) {
    const Token *token = peek_token(parser);
    if (token == NULL) return 0;

    if (token->type == TOKEN_REGISTRO) return parse_record(parser);
    if (token->type == TOKEN_OUTRAFUNCAO) return parse_function(parser);

    if (is_return_type(token->type)) {
        if (parser->current + 1U < parser->tokens->count &&
            parser->tokens->items[parser->current + 1U].type == TOKEN_PRINCIPAL)
            return parse_principal(parser);
        if (is_value_type(token->type)) return parse_variable_declaration(parser);
        parser_error_at(parser, token,
                        "'vazio' so pode aparecer como tipo de retorno de funcao/principal");
        return 0;
    }

    if (token->type == TOKEN_FUNC) {
        parser_error_at(parser, token,
                        "'func' consta na lista de reservadas, mas a especificacao nao define uma producao sintatica para ela; use 'outrafuncao' para funcao nao-principal");
        return 0;
    }

    parser_error_at(parser, token,
                    "token nao inicia uma declaracao de nivel global");
    return 0;
}

ParserResult parser_parse(const TokenList *tokens, FILE *error_stream) {
    Parser parser;
    ParserResult result;

    parser.tokens = tokens;
    parser.current = 0U;
    parser.error_count = 0U;
    parser.error_stream = error_stream != NULL ? error_stream : stderr;

    if (tokens == NULL || tokens->count == 0U) {
        parser_error_at(&parser, NULL, "lista de tokens vazia ou inexistente");
    } else {
        while (!is_at_end(&parser)) {
            size_t before = parser.current;
            if (!parse_top_level(&parser)) {
                synchronize_top_level(&parser);
                if (parser.current == before && !is_at_end(&parser))
                    advance_token(&parser);
            }
        }
    }

    result.error_count = parser.error_count;
    result.status = parser.error_count == 0U ? PARSER_OK : PARSER_HAS_ERRORS;
    return result;
}

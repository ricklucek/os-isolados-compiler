#include "parser.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    const TokenList *tokens;
    size_t current;
    size_t error_count;
    int memory_error;
    FILE *error_stream;
} Parser;

static AstNode *parse_top_level(Parser *parser);
static AstNode *parse_statement(Parser *parser);
static AstNode *parse_block(Parser *parser);
static AstNode *parse_expression(Parser *parser);
static AstNode *parse_condition(Parser *parser);
static AstNode *parse_assignment_core(Parser *parser);

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

static const Token *match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return NULL;
    return advance_token(parser);
}

static void parser_error_at(Parser *parser, const Token *token,
                            const char *format, ...) {
    va_list args;
    FILE *stream = parser->error_stream != NULL ? parser->error_stream : stderr;

    ++parser->error_count;
    if (token == NULL) {
        fprintf(stream, "[ERRO SINTATICO] fim inesperado da entrada: ");
    } else if (token->type == TOKEN_EOF) {
        fprintf(stream,
                "[ERRO SINTATICO] linha %d, coluna %d, no fim do arquivo: ",
                token->line, token->column);
    } else {
        fprintf(stream,
                "[ERRO SINTATICO] linha %d, coluna %d, token '%s' (%s): ",
                token->line, token->column, token->lexeme,
                token_type_name(token->type));
    }

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fputc('\n', stream);
}

static void parser_memory_error(Parser *parser) {
    if (!parser->memory_error) {
        FILE *stream = parser->error_stream != NULL ? parser->error_stream : stderr;
        fprintf(stream,
                "[ERRO INTERNO] memoria insuficiente durante a construcao da AST.\n");
    }
    parser->memory_error = 1;
}

static const Token *consume(Parser *parser, TokenType type,
                            const char *message) {
    const Token *token;
    if (check(parser, type)) {
        token = advance_token(parser);
        return token;
    }
    parser_error_at(parser, peek_token(parser), "%s; esperado %s.",
                    message, token_type_name(type));
    return NULL;
}

static int is_value_type(TokenType type) {
    return type == TOKEN_BOOL || type == TOKEN_INTEIRA ||
           type == TOKEN_FLUT || type == TOKEN_PALAVRA ||
           type == TOKEN_DUPLOCARPADO;
}

static int is_return_type(TokenType type) {
    return is_value_type(type) || type == TOKEN_VAZIO;
}

static int starts_statement(TokenType type) {
    return is_value_type(type) || type == TOKEN_IDENTIFIER ||
           type == TOKEN_RESPOST || type == TOKEN_COND ||
           type == TOKEN_DURANTE || type == TOKEN_REPETE;
}

static int starts_top_level(TokenType type) {
    return type == TOKEN_REGISTRO || type == TOKEN_OUTRAFUNCAO ||
           is_return_type(type);
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
    while (!is_at_end(parser) && !check(parser, type))
        advance_token(parser);
}

static AstNode *new_node(Parser *parser, AstNodeKind kind,
                         TokenType type, const char *lexeme,
                         int line, int column) {
    AstNode *node = ast_node_create(kind, type, lexeme, line, column);
    if (node == NULL) parser_memory_error(parser);
    return node;
}

static AstNode *node_from_token(Parser *parser, AstNodeKind kind,
                                const Token *token) {
    AstNode *node = ast_node_from_token(kind, token);
    if (node == NULL) parser_memory_error(parser);
    return node;
}

static int add_child(Parser *parser, AstNode *parent, AstNode *child) {
    if (parent == NULL || child == NULL) return 0;
    if (!ast_node_add_child(parent, child)) {
        parser_memory_error(parser);
        return 0;
    }
    return 1;
}

static AstNode *make_binary(Parser *parser, const Token *op,
                            AstNode *left, AstNode *right) {
    AstNode *node;
    if (left == NULL || right == NULL) {
        ast_free(left);
        ast_free(right);
        return NULL;
    }

    node = node_from_token(parser, AST_BINARY_EXPR, op);
    if (node == NULL) {
        ast_free(left);
        ast_free(right);
        return NULL;
    }
    if (!add_child(parser, node, left)) {
        ast_free(left);
        ast_free(right);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, right)) {
        ast_free(node);
        ast_free(right);
        return NULL;
    }
    return node;
}

static AstNode *make_unary(Parser *parser, const Token *op,
                           AstNode *operand) {
    AstNode *node;
    if (operand == NULL) return NULL;
    node = node_from_token(parser, AST_UNARY_EXPR, op);
    if (node == NULL) {
        ast_free(operand);
        return NULL;
    }
    if (!add_child(parser, node, operand)) {
        ast_free(node);
        ast_free(operand);
        return NULL;
    }
    return node;
}

static const Token *parse_value_type(Parser *parser, const char *context) {
    const Token *token = peek_token(parser);
    if (token != NULL && is_value_type(token->type))
        return advance_token(parser);

    parser_error_at(
        parser, token,
        "%s exige um tipo de valor (bool, inteira, flut, palavra ou duplocarpado)",
        context);
    return NULL;
}

static const Token *parse_return_type(Parser *parser, const char *context) {
    const Token *token = peek_token(parser);
    if (token != NULL && is_return_type(token->type))
        return advance_token(parser);

    parser_error_at(parser, token, "%s exige um tipo de retorno valido",
                    context);
    return NULL;
}

static AstNode *parse_variable_declaration(Parser *parser) {
    const Token *type_token;
    const Token *start_token;
    const Token *size_token = NULL;
    AstNode *node = NULL;
    AstNodeKind kind = AST_VAR_DECL;

    type_token = parse_value_type(parser, "declaracao");
    if (type_token == NULL) return NULL;
    start_token = type_token;

    if (match(parser, TOKEN_VET) != NULL) {
        kind = AST_VECTOR_DECL;
        if (consume(parser, TOKEN_LEFT_BRACE,
                    "declaracao de vetor exige '{' depois de vet") == NULL)
            return NULL;

        size_token = match(parser, TOKEN_INTEGER_LITERAL);
        if (size_token == NULL) {
            parser_error_at(
                parser, peek_token(parser),
                "tamanho do vetor deve ser um literal inteiro; esperado %s",
                token_type_name(TOKEN_INTEGER_LITERAL));
            recover_until(parser, TOKEN_RIGHT_BRACE);
            consume(parser, TOKEN_RIGHT_BRACE,
                    "declaracao de vetor exige '}' depois do tamanho");
            return NULL;
        }

        if (consume(parser, TOKEN_RIGHT_BRACE,
                    "declaracao de vetor exige '}' depois do tamanho") == NULL)
            return NULL;
    }

    node = new_node(parser, kind, type_token->type, NULL,
                    start_token->line, start_token->column);
    if (node == NULL) return NULL;

    if (size_token != NULL) {
        AstNode *size_node =
            node_from_token(parser, AST_INTEGER_LITERAL, size_token);
        if (size_node == NULL || !add_child(parser, node, size_node)) {
            if (size_node != NULL) ast_free(size_node);
            ast_free(node);
            return NULL;
        }
    }

    for (;;) {
        const Token *name =
            consume(parser, TOKEN_IDENTIFIER,
                    "declaracao exige um identificador");
        AstNode *identifier;
        if (name == NULL) {
            ast_free(node);
            return NULL;
        }

        identifier = node_from_token(parser, AST_IDENTIFIER, name);
        if (identifier == NULL || !add_child(parser, node, identifier)) {
            if (identifier != NULL) ast_free(identifier);
            ast_free(node);
            return NULL;
        }

        if (match(parser, TOKEN_COMMA) == NULL) break;
    }

    if (consume(parser, TOKEN_SEMICOLON,
                "declaracao deve terminar com ';'") == NULL) {
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_parameter(Parser *parser) {
    const Token *type_token = parse_value_type(parser, "parametro");
    const Token *name;
    if (type_token == NULL) return NULL;
    name = consume(parser, TOKEN_IDENTIFIER, "parametro exige um nome");
    if (name == NULL) return NULL;

    return new_node(parser, AST_PARAMETER, type_token->type, name->lexeme,
                    type_token->line, type_token->column);
}

static int parse_parameter_list_into(Parser *parser, AstNode *function) {
    if (check(parser, TOKEN_RIGHT_PAREN)) return 1;

    for (;;) {
        AstNode *parameter = parse_parameter(parser);
        if (parameter == NULL) return 0;
        if (!add_child(parser, function, parameter)) {
            ast_free(parameter);
            return 0;
        }
        if (match(parser, TOKEN_COMMA) == NULL) break;
    }
    return 1;
}

static int parse_argument_list_into(Parser *parser, AstNode *call) {
    if (check(parser, TOKEN_RIGHT_PAREN)) return 1;

    for (;;) {
        AstNode *argument = parse_expression(parser);
        if (argument == NULL) return 0;
        if (!add_child(parser, call, argument)) {
            ast_free(argument);
            return 0;
        }
        if (match(parser, TOKEN_COMMA) == NULL) break;
    }
    return 1;
}

static AstNode *parse_primary(Parser *parser) {
    const Token *token;

    token = match(parser, TOKEN_INTEGER_LITERAL);
    if (token != NULL)
        return node_from_token(parser, AST_INTEGER_LITERAL, token);

    token = match(parser, TOKEN_REAL_LITERAL);
    if (token != NULL)
        return node_from_token(parser, AST_REAL_LITERAL, token);

    token = match(parser, TOKEN_STRING_LITERAL);
    if (token != NULL)
        return node_from_token(parser, AST_STRING_LITERAL, token);

    token = match(parser, TOKEN_VER);
    if (token == NULL) token = match(parser, TOKEN_FAL);
    if (token != NULL)
        return node_from_token(parser, AST_BOOL_LITERAL, token);

    token = match(parser, TOKEN_RAIZ);
    if (token != NULL) {
        AstNode *root;
        AstNode *argument;
        if (consume(parser, TOKEN_LEFT_PAREN, "raiz exige '('") == NULL)
            return NULL;
        argument = parse_expression(parser);
        if (argument == NULL) return NULL;
        if (consume(parser, TOKEN_RIGHT_PAREN,
                    "raiz exige ')' depois do argumento") == NULL) {
            ast_free(argument);
            return NULL;
        }

        root = node_from_token(parser, AST_ROOT_EXPR, token);
        if (root == NULL) {
            ast_free(argument);
            return NULL;
        }
        if (!add_child(parser, root, argument)) {
            ast_free(argument);
            ast_free(root);
            return NULL;
        }
        return root;
    }

    token = match(parser, TOKEN_IDENTIFIER);
    if (token != NULL) {
        if (match(parser, TOKEN_LEFT_PAREN) != NULL) {
            AstNode *call = new_node(parser, AST_CALL, TOKEN_IDENTIFIER,
                                     token->lexeme, token->line, token->column);
            if (call == NULL) return NULL;
            if (!parse_argument_list_into(parser, call)) {
                ast_free(call);
                return NULL;
            }
            if (consume(parser, TOKEN_RIGHT_PAREN,
                        "chamada de funcao exige ')'") == NULL) {
                ast_free(call);
                return NULL;
            }
            return call;
        }

        if (match(parser, TOKEN_LEFT_BRACE) != NULL) {
            AstNode *access = new_node(
                parser, AST_VECTOR_ACCESS, TOKEN_IDENTIFIER,
                token->lexeme, token->line, token->column);
            AstNode *index;
            if (access == NULL) return NULL;
            index = parse_expression(parser);
            if (index == NULL) {
                ast_free(access);
                return NULL;
            }
            if (consume(parser, TOKEN_RIGHT_BRACE,
                        "acesso a vetor exige '}' depois do indice") == NULL) {
                ast_free(index);
                ast_free(access);
                return NULL;
            }
            if (!add_child(parser, access, index)) {
                ast_free(index);
                ast_free(access);
                return NULL;
            }
            return access;
        }

        return node_from_token(parser, AST_IDENTIFIER, token);
    }

    if (match(parser, TOKEN_LEFT_PAREN) != NULL) {
        AstNode *expression = parse_expression(parser);
        if (expression == NULL) return NULL;
        if (consume(parser, TOKEN_RIGHT_PAREN,
                    "expressao agrupada exige ')'") == NULL) {
            ast_free(expression);
            return NULL;
        }
        return expression;
    }

    parser_error_at(parser, peek_token(parser), "esperada expressao");
    return NULL;
}

static AstNode *parse_unary(Parser *parser) {
    const Token *op = match(parser, TOKEN_MINUS);
    if (op == NULL) op = match(parser, TOKEN_PLUS);
    if (op == NULL) op = match(parser, TOKEN_NAO);
    if (op != NULL) {
        AstNode *operand = parse_unary(parser);
        return make_unary(parser, op, operand);
    }
    return parse_primary(parser);
}

static AstNode *parse_power(Parser *parser) {
    AstNode *left = parse_unary(parser);
    const Token *op;
    AstNode *right;
    if (left == NULL) return NULL;

    op = match(parser, TOKEN_CARET);
    if (op == NULL) return left;

    right = parse_power(parser);
    if (right == NULL) {
        ast_free(left);
        return NULL;
    }
    return make_binary(parser, op, left, right);
}

static AstNode *parse_factor(Parser *parser) {
    AstNode *left = parse_power(parser);
    if (left == NULL) return NULL;

    for (;;) {
        const Token *op = match(parser, TOKEN_STAR);
        AstNode *right;
        AstNode *combined;
        if (op == NULL) op = match(parser, TOKEN_SLASH);
        if (op == NULL) op = match(parser, TOKEN_INTEGER_DIV);
        if (op == NULL) break;

        right = parse_power(parser);
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_term(Parser *parser) {
    AstNode *left = parse_factor(parser);
    if (left == NULL) return NULL;

    for (;;) {
        const Token *op = match(parser, TOKEN_PLUS);
        AstNode *right;
        AstNode *combined;
        if (op == NULL) op = match(parser, TOKEN_MINUS);
        if (op == NULL) break;

        right = parse_factor(parser);
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_comparison_expression(Parser *parser) {
    AstNode *left = parse_term(parser);
    if (left == NULL) return NULL;

    for (;;) {
        const Token *op = match(parser, TOKEN_LESS);
        AstNode *right;
        AstNode *combined;
        if (op == NULL) op = match(parser, TOKEN_GREATER);
        if (op == NULL) break;

        right = parse_term(parser);
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_equality(Parser *parser) {
    AstNode *left = parse_comparison_expression(parser);
    if (left == NULL) return NULL;

    for (;;) {
        const Token *op = match(parser, TOKEN_EQUAL_EQUAL);
        AstNode *right;
        AstNode *combined;
        if (op == NULL) op = match(parser, TOKEN_BANG);
        if (op == NULL) break;

        right = parse_comparison_expression(parser);
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_logical_and(Parser *parser) {
    AstNode *left = parse_equality(parser);
    if (left == NULL) return NULL;

    while (check(parser, TOKEN_E)) {
        const Token *op = advance_token(parser);
        AstNode *right = parse_equality(parser);
        AstNode *combined;
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_logical_xor(Parser *parser) {
    AstNode *left = parse_logical_and(parser);
    if (left == NULL) return NULL;

    while (check(parser, TOKEN_XOR)) {
        const Token *op = advance_token(parser);
        AstNode *right = parse_logical_and(parser);
        AstNode *combined;
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_expression(Parser *parser) {
    AstNode *left = parse_logical_xor(parser);
    if (left == NULL) return NULL;

    while (check(parser, TOKEN_OU)) {
        const Token *op = advance_token(parser);
        AstNode *right = parse_logical_xor(parser);
        AstNode *combined;
        if (right == NULL) {
            ast_free(left);
            return NULL;
        }
        combined = make_binary(parser, op, left, right);
        if (combined == NULL) return NULL;
        left = combined;
    }
    return left;
}

static AstNode *parse_condition_operand(Parser *parser) {
    const Token *token;

    token = match(parser, TOKEN_INTEGER_LITERAL);
    if (token != NULL)
        return node_from_token(parser, AST_INTEGER_LITERAL, token);

    token = match(parser, TOKEN_REAL_LITERAL);
    if (token != NULL)
        return node_from_token(parser, AST_REAL_LITERAL, token);

    token = match(parser, TOKEN_IDENTIFIER);
    if (token != NULL) {
        if (match(parser, TOKEN_LEFT_BRACE) != NULL) {
            AstNode *access = new_node(
                parser, AST_VECTOR_ACCESS, TOKEN_IDENTIFIER,
                token->lexeme, token->line, token->column);
            AstNode *index;
            if (access == NULL) return NULL;
            index = parse_expression(parser);
            if (index == NULL) {
                ast_free(access);
                return NULL;
            }
            if (consume(parser, TOKEN_RIGHT_BRACE,
                        "acesso a vetor na condicao exige '}'") == NULL) {
                ast_free(index);
                ast_free(access);
                return NULL;
            }
            if (!add_child(parser, access, index)) {
                ast_free(index);
                ast_free(access);
                return NULL;
            }
            return access;
        }
        return node_from_token(parser, AST_IDENTIFIER, token);
    }

    parser_error_at(
        parser, peek_token(parser),
        "condicao exige VAR ou NUM como operando, conforme a especificacao");
    return NULL;
}

static AstNode *parse_condition(Parser *parser) {
    AstNode *left = parse_condition_operand(parser);
    const Token *op;
    AstNode *right;

    if (left == NULL) return NULL;

    op = match(parser, TOKEN_LESS);
    if (op == NULL) op = match(parser, TOKEN_GREATER);
    if (op == NULL) op = match(parser, TOKEN_EQUAL_EQUAL);
    if (op == NULL) op = match(parser, TOKEN_BANG);

    if (op == NULL) {
        parser_error_at(parser, peek_token(parser),
                        "condicao exige comparador '<', '>', '==' ou '!'");
        ast_free(left);
        return NULL;
    }

    right = parse_condition_operand(parser);
    if (right == NULL) {
        ast_free(left);
        return NULL;
    }
    return make_binary(parser, op, left, right);
}

static AstNode *parse_lvalue(Parser *parser) {
    const Token *name =
        consume(parser, TOKEN_IDENTIFIER,
                "atribuicao exige identificador no lado esquerdo");
    if (name == NULL) return NULL;

    if (match(parser, TOKEN_LEFT_BRACE) != NULL) {
        AstNode *access =
            new_node(parser, AST_VECTOR_ACCESS, TOKEN_IDENTIFIER,
                     name->lexeme, name->line, name->column);
        AstNode *index;
        if (access == NULL) return NULL;
        index = parse_expression(parser);
        if (index == NULL) {
            ast_free(access);
            return NULL;
        }
        if (consume(parser, TOKEN_RIGHT_BRACE,
                    "acesso a vetor exige '}' depois do indice") == NULL) {
            ast_free(index);
            ast_free(access);
            return NULL;
        }
        if (!add_child(parser, access, index)) {
            ast_free(index);
            ast_free(access);
            return NULL;
        }
        return access;
    }

    return node_from_token(parser, AST_IDENTIFIER, name);
}

static AstNode *parse_assignment_core(Parser *parser) {
    AstNode *target = parse_lvalue(parser);
    const Token *receba;
    AstNode *value;
    AstNode *assignment;

    if (target == NULL) return NULL;

    receba = consume(parser, TOKEN_RECEBA,
                     "atribuicao exige a palavra 'receba'");
    if (receba == NULL) {
        ast_free(target);
        return NULL;
    }

    value = parse_expression(parser);
    if (value == NULL) {
        ast_free(target);
        return NULL;
    }

    assignment = node_from_token(parser, AST_ASSIGNMENT, receba);
    if (assignment == NULL) {
        ast_free(target);
        ast_free(value);
        return NULL;
    }

    if (!add_child(parser, assignment, target)) {
        ast_free(target);
        ast_free(value);
        ast_free(assignment);
        return NULL;
    }
    if (!add_child(parser, assignment, value)) {
        ast_free(value);
        ast_free(assignment);
        return NULL;
    }
    return assignment;
}

static AstNode *parse_identifier_statement(Parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) return NULL;

    if (parser->current + 1U < parser->tokens->count &&
        parser->tokens->items[parser->current + 1U].type ==
            TOKEN_LEFT_PAREN) {
        const Token *name = advance_token(parser);
        AstNode *call =
            new_node(parser, AST_CALL, TOKEN_IDENTIFIER, name->lexeme,
                     name->line, name->column);
        if (call == NULL) return NULL;

        advance_token(parser); /* '(' */
        if (!parse_argument_list_into(parser, call)) {
            ast_free(call);
            return NULL;
        }
        if (consume(parser, TOKEN_RIGHT_PAREN,
                    "chamada de funcao exige ')'") == NULL ||
            consume(parser, TOKEN_SEMICOLON,
                    "chamada de funcao deve terminar com ';'") == NULL) {
            ast_free(call);
            return NULL;
        }
        return call;
    }

    {
        AstNode *assignment = parse_assignment_core(parser);
        if (assignment == NULL) return NULL;
        if (consume(parser, TOKEN_SEMICOLON,
                    "atribuicao deve terminar com ';'") == NULL) {
            ast_free(assignment);
            return NULL;
        }
        return assignment;
    }
}

static AstNode *parse_return_statement(Parser *parser) {
    const Token *respost =
        consume(parser, TOKEN_RESPOST, "esperado 'respost'");
    AstNode *node;
    if (respost == NULL) return NULL;

    node = node_from_token(parser, AST_RETURN, respost);
    if (node == NULL) return NULL;

    if (match(parser, TOKEN_SEMICOLON) != NULL) return node;

    {
        AstNode *value = parse_expression(parser);
        if (value == NULL) {
            ast_free(node);
            return NULL;
        }
        if (!add_child(parser, node, value)) {
            ast_free(value);
            ast_free(node);
            return NULL;
        }
    }

    if (consume(parser, TOKEN_SEMICOLON,
                "retorno deve terminar com ';'") == NULL) {
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_conditional(Parser *parser) {
    const Token *cond =
        consume(parser, TOKEN_COND, "esperado 'cond'");
    AstNode *node;
    AstNode *condition;
    AstNode *block;

    if (cond == NULL) return NULL;
    node = node_from_token(parser, AST_IF, cond);
    if (node == NULL) return NULL;

    if (consume(parser, TOKEN_LEFT_PAREN, "cond exige '('") == NULL) {
        ast_free(node);
        return NULL;
    }

    condition = parse_condition(parser);
    if (condition == NULL) {
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "cond exige ')' depois da condicao");
        ast_free(node);
        return NULL;
    }

    if (consume(parser, TOKEN_RIGHT_PAREN,
                "cond exige ')' depois da condicao") == NULL ||
        consume(parser, TOKEN_EDAI, "cond exige 'edai'") == NULL) {
        ast_free(condition);
        ast_free(node);
        return NULL;
    }

    block = parse_block(parser);
    if (block == NULL) {
        ast_free(condition);
        ast_free(node);
        return NULL;
    }

    if (!add_child(parser, node, condition)) {
        ast_free(condition);
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, block)) {
        ast_free(block);
        ast_free(node);
        return NULL;
    }

    while (check(parser, TOKEN_CASOCONTRARIO)) {
        const Token *otherwise = advance_token(parser);

        if (match(parser, TOKEN_COND) != NULL) {
            AstNode *else_if =
                node_from_token(parser, AST_ELSE_IF, otherwise);
            AstNode *else_condition;
            AstNode *else_block;
            if (else_if == NULL) {
                ast_free(node);
                return NULL;
            }

            if (consume(parser, TOKEN_LEFT_PAREN,
                        "casocontrario cond exige '('") == NULL) {
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }

            else_condition = parse_condition(parser);
            if (else_condition == NULL) {
                recover_until(parser, TOKEN_RIGHT_PAREN);
                consume(parser, TOKEN_RIGHT_PAREN,
                        "casocontrario cond exige ')'");
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }

            if (consume(parser, TOKEN_RIGHT_PAREN,
                        "casocontrario cond exige ')'") == NULL ||
                consume(parser, TOKEN_EDAI,
                        "casocontrario cond exige 'edai'") == NULL) {
                ast_free(else_condition);
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }

            else_block = parse_block(parser);
            if (else_block == NULL) {
                ast_free(else_condition);
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }

            if (!add_child(parser, else_if, else_condition)) {
                ast_free(else_condition);
                ast_free(else_block);
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }
            if (!add_child(parser, else_if, else_block)) {
                ast_free(else_block);
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }
            if (!add_child(parser, node, else_if)) {
                ast_free(else_if);
                ast_free(node);
                return NULL;
            }
        } else {
            AstNode *else_node =
                node_from_token(parser, AST_ELSE, otherwise);
            AstNode *else_block;
            if (else_node == NULL) {
                ast_free(node);
                return NULL;
            }

            if (consume(parser, TOKEN_EDAI,
                        "casocontrario exige 'edai'") == NULL) {
                ast_free(else_node);
                ast_free(node);
                return NULL;
            }

            else_block = parse_block(parser);
            if (else_block == NULL) {
                ast_free(else_node);
                ast_free(node);
                return NULL;
            }
            if (!add_child(parser, else_node, else_block)) {
                ast_free(else_block);
                ast_free(else_node);
                ast_free(node);
                return NULL;
            }
            if (!add_child(parser, node, else_node)) {
                ast_free(else_node);
                ast_free(node);
                return NULL;
            }
            break;
        }
    }
    return node;
}

static AstNode *parse_while(Parser *parser) {
    const Token *during =
        consume(parser, TOKEN_DURANTE, "esperado 'durante'");
    AstNode *node;
    AstNode *condition;
    AstNode *block;

    if (during == NULL) return NULL;
    node = node_from_token(parser, AST_WHILE, during);
    if (node == NULL) return NULL;

    if (consume(parser, TOKEN_LEFT_PAREN, "durante exige '('") == NULL) {
        ast_free(node);
        return NULL;
    }

    condition = parse_condition(parser);
    if (condition == NULL) {
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "durante exige ')' depois da condicao");
        ast_free(node);
        return NULL;
    }

    if (consume(parser, TOKEN_RIGHT_PAREN,
                "durante exige ')' depois da condicao") == NULL ||
        consume(parser, TOKEN_EDAI, "durante exige 'edai'") == NULL) {
        ast_free(condition);
        ast_free(node);
        return NULL;
    }

    block = parse_block(parser);
    if (block == NULL) {
        ast_free(condition);
        ast_free(node);
        return NULL;
    }

    if (!add_child(parser, node, condition)) {
        ast_free(condition);
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, block)) {
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_for(Parser *parser) {
    const Token *repeat =
        consume(parser, TOKEN_REPETE, "esperado 'repete'");
    AstNode *node;
    AstNode *initialization;
    AstNode *condition;
    AstNode *iteration;
    AstNode *block;

    if (repeat == NULL) return NULL;
    node = node_from_token(parser, AST_FOR, repeat);
    if (node == NULL) return NULL;

    if (consume(parser, TOKEN_LEFT_PAREN, "repete exige '('") == NULL) {
        ast_free(node);
        return NULL;
    }

    initialization = parse_assignment_core(parser);
    if (initialization == NULL ||
        consume(parser, TOKEN_COMMA,
                "repete separa inicializacao e condicao com ','") == NULL) {
        ast_free(initialization);
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "repete exige ')' depois da iteracao");
        ast_free(node);
        return NULL;
    }

    condition = parse_condition(parser);
    if (condition == NULL ||
        consume(parser, TOKEN_COMMA,
                "repete separa condicao e iteracao com ','") == NULL) {
        ast_free(initialization);
        ast_free(condition);
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "repete exige ')' depois da iteracao");
        ast_free(node);
        return NULL;
    }

    iteration = parse_assignment_core(parser);
    if (iteration == NULL ||
        consume(parser, TOKEN_RIGHT_PAREN,
                "repete exige ')' depois da iteracao") == NULL ||
        consume(parser, TOKEN_EDAI, "repete exige 'edai'") == NULL) {
        ast_free(initialization);
        ast_free(condition);
        ast_free(iteration);
        ast_free(node);
        return NULL;
    }

    block = parse_block(parser);
    if (block == NULL) {
        ast_free(initialization);
        ast_free(condition);
        ast_free(iteration);
        ast_free(node);
        return NULL;
    }

    if (!add_child(parser, node, initialization)) {
        ast_free(initialization);
        ast_free(condition);
        ast_free(iteration);
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, condition)) {
        ast_free(condition);
        ast_free(iteration);
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, iteration)) {
        ast_free(iteration);
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, block)) {
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_statement(Parser *parser) {
    const Token *token = peek_token(parser);
    if (token == NULL) return NULL;

    if (is_value_type(token->type))
        return parse_variable_declaration(parser);
    if (token->type == TOKEN_IDENTIFIER)
        return parse_identifier_statement(parser);
    if (token->type == TOKEN_RESPOST)
        return parse_return_statement(parser);
    if (token->type == TOKEN_COND)
        return parse_conditional(parser);
    if (token->type == TOKEN_DURANTE)
        return parse_while(parser);
    if (token->type == TOKEN_REPETE)
        return parse_for(parser);

    parser_error_at(parser, token,
                    "token nao inicia um comando valido");
    return NULL;
}

static AstNode *parse_block(Parser *parser) {
    const Token *left =
        consume(parser, TOKEN_LEFT_BRACKET, "bloco exige '['");
    AstNode *block;

    if (left == NULL) return NULL;
    block = new_node(parser, AST_BLOCK, TOKEN_LEFT_BRACKET, NULL,
                     left->line, left->column);
    if (block == NULL) return NULL;

    while (!check(parser, TOKEN_RIGHT_BRACKET) && !is_at_end(parser) &&
           !parser->memory_error) {
        size_t before = parser->current;
        AstNode *statement = parse_statement(parser);

        if (statement != NULL) {
            if (!add_child(parser, block, statement)) {
                ast_free(statement);
                ast_free(block);
                return NULL;
            }
        } else {
            synchronize_statement(parser);
            if (parser->current == before && !is_at_end(parser) &&
                !check(parser, TOKEN_RIGHT_BRACKET))
                advance_token(parser);
        }
    }

    if (consume(parser, TOKEN_RIGHT_BRACKET, "bloco exige ']'") == NULL) {
        ast_free(block);
        return NULL;
    }
    return block;
}

static AstNode *parse_record(Parser *parser) {
    const Token *record =
        consume(parser, TOKEN_REGISTRO, "esperado 'registro'");
    const Token *name;
    AstNode *node;

    if (record == NULL) return NULL;
    name = consume(parser, TOKEN_IDENTIFIER, "registro exige um nome");
    if (name == NULL ||
        consume(parser, TOKEN_AT, "registro exige '@'") == NULL ||
        consume(parser, TOKEN_LEFT_BRACKET, "registro exige '['") == NULL)
        return NULL;

    node = new_node(parser, AST_RECORD, TOKEN_REGISTRO, name->lexeme,
                    record->line, record->column);
    if (node == NULL) return NULL;

    while (!check(parser, TOKEN_RIGHT_BRACKET) && !is_at_end(parser) &&
           !parser->memory_error) {
        size_t before = parser->current;
        AstNode *declaration = parse_variable_declaration(parser);

        if (declaration != NULL) {
            if (!add_child(parser, node, declaration)) {
                ast_free(declaration);
                ast_free(node);
                return NULL;
            }
        } else {
            synchronize_statement(parser);
            if (parser->current == before && !is_at_end(parser) &&
                !check(parser, TOKEN_RIGHT_BRACKET))
                advance_token(parser);
        }
    }

    if (consume(parser, TOKEN_RIGHT_BRACKET, "registro exige ']'") == NULL) {
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_function(Parser *parser) {
    const Token *function =
        consume(parser, TOKEN_OUTRAFUNCAO, "esperado 'outrafuncao'");
    const Token *return_type;
    const Token *name;
    AstNode *node;
    AstNode *block;

    if (function == NULL) return NULL;
    return_type = parse_return_type(parser, "outrafuncao");
    if (return_type == NULL) return NULL;
    name = consume(parser, TOKEN_IDENTIFIER, "funcao exige um nome");
    if (name == NULL) return NULL;

    node = new_node(parser, AST_FUNCTION, return_type->type, name->lexeme,
                    function->line, function->column);
    if (node == NULL) return NULL;

    if (consume(parser, TOKEN_LEFT_PAREN, "funcao exige '('") == NULL) {
        ast_free(node);
        return NULL;
    }

    if (!parse_parameter_list_into(parser, node)) {
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "funcao exige ')' depois dos parametros");
        ast_free(node);
        return NULL;
    }

    if (consume(parser, TOKEN_RIGHT_PAREN,
                "funcao exige ')' depois dos parametros") == NULL ||
        consume(parser, TOKEN_AT,
                "funcao exige '@' antes do bloco") == NULL) {
        ast_free(node);
        return NULL;
    }

    block = parse_block(parser);
    if (block == NULL) {
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, block)) {
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_principal(Parser *parser) {
    const Token *return_type =
        parse_return_type(parser, "principal");
    const Token *principal_token;
    AstNode *node;
    AstNode *block;

    if (return_type == NULL) return NULL;
    principal_token =
        consume(parser, TOKEN_PRINCIPAL, "esperado 'principal'");
    if (principal_token == NULL) return NULL;

    node = new_node(parser, AST_PRINCIPAL, return_type->type,
                    principal_token->lexeme, principal_token->line,
                    principal_token->column);
    if (node == NULL) return NULL;

    if (consume(parser, TOKEN_LEFT_PAREN,
                "principal exige '('") == NULL) {
        ast_free(node);
        return NULL;
    }

    if (!parse_parameter_list_into(parser, node)) {
        recover_until(parser, TOKEN_RIGHT_PAREN);
        consume(parser, TOKEN_RIGHT_PAREN,
                "principal exige ')' depois dos parametros");
        ast_free(node);
        return NULL;
    }

    if (consume(parser, TOKEN_RIGHT_PAREN,
                "principal exige ')' depois dos parametros") == NULL ||
        consume(parser, TOKEN_AT,
                "principal exige '@' antes do bloco") == NULL) {
        ast_free(node);
        return NULL;
    }

    block = parse_block(parser);
    if (block == NULL) {
        ast_free(node);
        return NULL;
    }
    if (!add_child(parser, node, block)) {
        ast_free(block);
        ast_free(node);
        return NULL;
    }
    return node;
}

static AstNode *parse_top_level(Parser *parser) {
    const Token *token = peek_token(parser);
    if (token == NULL) return NULL;

    if (token->type == TOKEN_REGISTRO)
        return parse_record(parser);
    if (token->type == TOKEN_OUTRAFUNCAO)
        return parse_function(parser);

    if (is_return_type(token->type)) {
        if (parser->current + 1U < parser->tokens->count &&
            parser->tokens->items[parser->current + 1U].type ==
                TOKEN_PRINCIPAL)
            return parse_principal(parser);

        if (is_value_type(token->type))
            return parse_variable_declaration(parser);

        parser_error_at(
            parser, token,
            "'vazio' so pode aparecer como tipo de retorno de funcao/principal");
        return NULL;
    }

    if (token->type == TOKEN_FUNC) {
        parser_error_at(
            parser, token,
            "'func' consta na lista de reservadas, mas a especificacao nao define uma producao sintatica para ela; use 'outrafuncao' para funcao nao-principal");
        return NULL;
    }

    parser_error_at(parser, token,
                    "token nao inicia uma declaracao de nivel global");
    return NULL;
}

ParserResult parser_parse_ast(const TokenList *tokens, AstNode **out_ast,
                              FILE *error_stream) {
    Parser parser;
    ParserResult result;
    AstNode *program = NULL;
    const Token *first = NULL;

    if (out_ast != NULL) *out_ast = NULL;

    parser.tokens = tokens;
    parser.current = 0U;
    parser.error_count = 0U;
    parser.memory_error = 0;
    parser.error_stream = error_stream != NULL ? error_stream : stderr;

    if (tokens == NULL || tokens->count == 0U) {
        parser_error_at(&parser, NULL,
                        "lista de tokens vazia ou inexistente");
    } else {
        first = &tokens->items[0];
        program = new_node(&parser, AST_PROGRAM, TOKEN_INVALID, NULL,
                           first->line, first->column);

        while (program != NULL && !is_at_end(&parser) &&
               !parser.memory_error) {
            size_t before = parser.current;
            AstNode *top_level = parse_top_level(&parser);

            if (top_level != NULL) {
                if (!add_child(&parser, program, top_level)) {
                    ast_free(top_level);
                    break;
                }
            } else {
                synchronize_top_level(&parser);
                if (parser.current == before && !is_at_end(&parser))
                    advance_token(&parser);
            }
        }
    }

    if (parser.memory_error) {
        ast_free(program);
        result.status = PARSER_MEMORY_ERROR;
        result.error_count = parser.error_count;
        return result;
    }

    if (parser.error_count != 0U) {
        ast_free(program);
        result.status = PARSER_HAS_ERRORS;
        result.error_count = parser.error_count;
        return result;
    }

    result.status = PARSER_OK;
    result.error_count = 0U;
    if (out_ast != NULL)
        *out_ast = program;
    else
        ast_free(program);
    return result;
}

ParserResult parser_parse(const TokenList *tokens, FILE *error_stream) {
    AstNode *ast = NULL;
    ParserResult result = parser_parse_ast(tokens, &ast, error_stream);
    ast_free(ast);
    return result;
}

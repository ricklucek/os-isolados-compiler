#include "semantic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

typedef struct {
    SymbolTable symbols;
    size_t error_count;
    int memory_error;
    FILE *error_stream;
    LangType current_return_type;
    const char *current_function_name;
    int saw_return;
} SemanticContext;

static LangType evaluate_expression(SemanticContext *context,
                                    const AstNode *node);
static LangType evaluate_lvalue(SemanticContext *context,
                                const AstNode *node);
static void analyze_statement(SemanticContext *context, const AstNode *node);
static void analyze_block(SemanticContext *context, const AstNode *block,
                          int create_scope, const char *scope_name);

static void semantic_error(SemanticContext *context, const AstNode *node,
                           const char *format, ...) {
    va_list args;
    FILE *stream;

    if (context == NULL) return;
    stream = context->error_stream != NULL ? context->error_stream : stderr;
    ++context->error_count;

    if (node != NULL)
        fprintf(stream, "[ERRO SEMANTICO] linha %d, coluna %d: ",
                node->line, node->column);
    else
        fprintf(stream, "[ERRO SEMANTICO] ");

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fputc('\n', stream);
}

static void semantic_memory_error(SemanticContext *context) {
    FILE *stream;
    if (context == NULL || context->memory_error) return;
    context->memory_error = 1;
    stream = context->error_stream != NULL ? context->error_stream : stderr;
    fprintf(stream,
            "[ERRO INTERNO] memoria insuficiente durante a analise semantica.\n");
}

static int enter_scope(SemanticContext *context, const char *name) {
    if (context->memory_error) return 0;
    if (!symbol_table_enter_scope(&context->symbols, name)) {
        semantic_memory_error(context);
        return 0;
    }
    return 1;
}

static int is_builtin_io_name(const char *name) {
    return name != NULL &&
           (strcmp(name, "entrada") == 0 || strcmp(name, "saida") == 0);
}

static int declare_symbol(SemanticContext *context, const AstNode *node,
                          const char *name, SymbolKind kind, LangType type,
                          size_t vector_size, const LangType *parameter_types,
                          size_t parameter_count) {
    SymbolDeclareStatus status;
    const Symbol *existing;

    if (context->memory_error) return 0;
    if (name == NULL || name[0] == '\0') {
        semantic_error(context, node, "simbolo sem nome nao pode ser declarado");
        return 0;
    }

    if (is_builtin_io_name(name)) {
        semantic_error(context, node,
                       "'%s' e reservado para a operacao embutida de entrada/saida",
                       name);
        return 0;
    }

    status = symbol_table_declare(
        &context->symbols, name, kind, type, vector_size, parameter_types,
        parameter_count, node != NULL ? node->line : 0,
        node != NULL ? node->column : 0, NULL);

    if (status == SYMBOL_DECLARE_OK) return 1;
    if (status == SYMBOL_DECLARE_MEMORY_ERROR) {
        semantic_memory_error(context);
        return 0;
    }

    existing = symbol_table_lookup_current(&context->symbols, name);
    if (existing != NULL) {
        semantic_error(context, node,
                       "'%s' ja foi declarado neste escopo como %s "
                       "(declaracao anterior em %d:%d)",
                       name, symbol_kind_name(existing->kind), existing->line,
                       existing->column);
    } else {
        semantic_error(context, node,
                       "'%s' possui declaracao duplicada neste escopo", name);
    }
    return 0;
}

static size_t function_parameter_count(const AstNode *function) {
    size_t i, count = 0U;
    if (function == NULL) return 0U;
    for (i = 0U; i < function->child_count; ++i)
        if (function->children[i] != NULL &&
            function->children[i]->kind == AST_PARAMETER)
            ++count;
    return count;
}

static void predeclare_callable(SemanticContext *context,
                                const AstNode *node) {
    LangType *parameter_types = NULL;
    size_t parameter_count;
    size_t parameter_index = 0U;
    size_t i;
    LangType return_type;
    SymbolKind kind;

    if (node == NULL || context->memory_error) return;

    if (node->kind == AST_RECORD) {
        declare_symbol(context, node, node->lexeme, SYMBOL_RECORD,
                       LANG_TYPE_ERROR, 0U, NULL, 0U);
        return;
    }

    if (node->kind != AST_FUNCTION && node->kind != AST_PRINCIPAL) return;

    parameter_count = function_parameter_count(node);
    if (parameter_count > 0U) {
        parameter_types = malloc(parameter_count * sizeof(*parameter_types));
        if (parameter_types == NULL) {
            semantic_memory_error(context);
            return;
        }
    }

    for (i = 0U; i < node->child_count; ++i) {
        const AstNode *child = node->children[i];
        if (child != NULL && child->kind == AST_PARAMETER)
            parameter_types[parameter_index++] =
                lang_type_from_token(child->token_type);
    }

    return_type = lang_type_from_token(node->token_type);
    kind = node->kind == AST_PRINCIPAL ? SYMBOL_PRINCIPAL : SYMBOL_FUNCTION;
    declare_symbol(context, node, node->lexeme, kind, return_type, 0U,
                   parameter_types, parameter_count);
    free(parameter_types);
}

static size_t vector_decl_first_identifier(const AstNode *node) {
    if (node != NULL && node->kind == AST_VECTOR_DECL &&
        node->child_count > 0U && node->children[0] != NULL &&
        node->children[0]->kind == AST_INTEGER_LITERAL)
        return 1U;
    return 0U;
}

static size_t vector_size_from_node(SemanticContext *context,
                                    const AstNode *node) {
    const AstNode *size_node;
    char *end = NULL;
    unsigned long value;

    if (node == NULL || node->kind != AST_VECTOR_DECL ||
        node->child_count == 0U)
        return 0U;

    size_node = node->children[0];
    if (size_node == NULL || size_node->lexeme == NULL) return 0U;

    value = strtoul(size_node->lexeme, &end, 10);
    if (end == size_node->lexeme || *end != '\0' || value == 0UL) {
        semantic_error(context, size_node,
                       "tamanho de vetor deve ser um inteiro positivo");
        return 0U;
    }
    return (size_t)value;
}

static void analyze_declaration(SemanticContext *context,
                                const AstNode *node) {
    LangType type;
    SymbolKind kind;
    size_t first_identifier;
    size_t vector_size = 0U;
    size_t i;

    if (node == NULL || context->memory_error) return;
    type = lang_type_from_token(node->token_type);
    if (type == LANG_TYPE_ERROR || type == LANG_TYPE_VAZIO) {
        semantic_error(context, node, "declaracao possui tipo invalido");
        return;
    }

    kind = node->kind == AST_VECTOR_DECL ? SYMBOL_VECTOR : SYMBOL_VARIABLE;
    first_identifier = vector_decl_first_identifier(node);
    if (node->kind == AST_VECTOR_DECL)
        vector_size = vector_size_from_node(context, node);

    for (i = first_identifier; i < node->child_count; ++i) {
        const AstNode *identifier = node->children[i];
        if (identifier == NULL || identifier->kind != AST_IDENTIFIER) continue;
        declare_symbol(context, identifier, identifier->lexeme, kind, type,
                       vector_size, NULL, 0U);
    }
}

static const Symbol *lookup_value_symbol(SemanticContext *context,
                                         const AstNode *node,
                                         const char *name) {
    const Symbol *symbol = symbol_table_lookup(&context->symbols, name);
    if (symbol == NULL) {
        semantic_error(context, node, "identificador '%s' usado sem declaracao",
                       name != NULL ? name : "<sem-nome>");
        return NULL;
    }
    return symbol;
}

static LangType evaluate_identifier(SemanticContext *context,
                                    const AstNode *node) {
    const Symbol *symbol =
        lookup_value_symbol(context, node, node != NULL ? node->lexeme : NULL);
    if (symbol == NULL) return LANG_TYPE_ERROR;

    if (symbol->kind == SYMBOL_VECTOR) {
        semantic_error(context, node,
                       "vetor '%s' deve ser acessado com um indice",
                       symbol->name);
        return LANG_TYPE_ERROR;
    }
    if (symbol->kind != SYMBOL_VARIABLE && symbol->kind != SYMBOL_PARAMETER) {
        semantic_error(context, node, "'%s' e %s e nao pode ser usado como valor",
                       symbol->name, symbol_kind_name(symbol->kind));
        return LANG_TYPE_ERROR;
    }
    return symbol->type;
}

static LangType evaluate_vector_access(SemanticContext *context,
                                       const AstNode *node) {
    const Symbol *symbol;
    LangType index_type = LANG_TYPE_ERROR;

    if (node == NULL) return LANG_TYPE_ERROR;
    symbol = lookup_value_symbol(context, node, node->lexeme);

    if (node->child_count > 0U)
        index_type = evaluate_expression(context, node->children[0]);
    else
        semantic_error(context, node, "acesso ao vetor '%s' nao possui indice",
                       node->lexeme != NULL ? node->lexeme : "<sem-nome>");

    if (index_type != LANG_TYPE_ERROR && index_type != LANG_TYPE_INTEIRA)
        semantic_error(context, node->children[0],
                       "indice de vetor deve ser do tipo inteira, encontrado %s",
                       lang_type_name(index_type));

    if (symbol == NULL) return LANG_TYPE_ERROR;
    if (symbol->kind != SYMBOL_VECTOR) {
        semantic_error(context, node, "'%s' nao foi declarado como vetor",
                       symbol->name);
        return LANG_TYPE_ERROR;
    }
    return symbol->type;
}

static LangType evaluate_call(SemanticContext *context, const AstNode *node) {
    const Symbol *symbol;
    size_t i;

    if (node == NULL) return LANG_TYPE_ERROR;

    if (node->lexeme != NULL && strcmp(node->lexeme, "entrada") == 0) {
        if (node->child_count != 1U) {
            semantic_error(context, node,
                           "entrada espera exatamente 1 destino, recebeu %zu",
                           node->child_count);
            for (i = 0U; i < node->child_count; ++i)
                evaluate_expression(context, node->children[i]);
            return LANG_TYPE_ERROR;
        }
        return evaluate_lvalue(context, node->children[0]) == LANG_TYPE_ERROR
                   ? LANG_TYPE_ERROR
                   : LANG_TYPE_VAZIO;
    }

    if (node->lexeme != NULL && strcmp(node->lexeme, "saida") == 0) {
        LangType value_type;
        if (node->child_count != 1U) {
            semantic_error(context, node,
                           "saida espera exatamente 1 expressao, recebeu %zu",
                           node->child_count);
            for (i = 0U; i < node->child_count; ++i)
                evaluate_expression(context, node->children[i]);
            return LANG_TYPE_ERROR;
        }
        value_type = evaluate_expression(context, node->children[0]);
        if (value_type == LANG_TYPE_ERROR) return LANG_TYPE_ERROR;
        if (value_type == LANG_TYPE_VAZIO) {
            semantic_error(context, node->children[0],
                           "saida nao pode receber expressao do tipo vazio");
            return LANG_TYPE_ERROR;
        }
        return LANG_TYPE_VAZIO;
    }

    symbol = symbol_table_lookup(&context->symbols, node->lexeme);

    if (symbol == NULL) {
        semantic_error(context, node, "funcao '%s' chamada sem declaracao",
                       node->lexeme != NULL ? node->lexeme : "<sem-nome>");
    } else if (symbol->kind != SYMBOL_FUNCTION &&
               symbol->kind != SYMBOL_PRINCIPAL) {
        semantic_error(context, node, "'%s' e %s e nao pode ser chamado como funcao",
                       symbol->name, symbol_kind_name(symbol->kind));
        symbol = NULL;
    }

    if (symbol != NULL && node->child_count != symbol->parameter_count) {
        semantic_error(context, node,
                       "funcao '%s' espera %zu argumento(s), recebeu %zu",
                       symbol->name, symbol->parameter_count, node->child_count);
    }

    for (i = 0U; i < node->child_count; ++i) {
        LangType argument_type = evaluate_expression(context, node->children[i]);
        if (symbol != NULL && i < symbol->parameter_count &&
            argument_type != LANG_TYPE_ERROR &&
            !lang_type_can_assign(symbol->parameter_types[i], argument_type)) {
            semantic_error(
                context, node->children[i],
                "argumento %zu de '%s' espera %s, encontrado %s", i + 1U,
                symbol->name, lang_type_name(symbol->parameter_types[i]),
                lang_type_name(argument_type));
        }
    }

    return symbol != NULL ? symbol->type : LANG_TYPE_ERROR;
}

static LangType evaluate_binary(SemanticContext *context,
                                const AstNode *node) {
    LangType left;
    LangType right;
    LangType common;

    if (node == NULL || node->child_count < 2U) return LANG_TYPE_ERROR;
    left = evaluate_expression(context, node->children[0]);
    right = evaluate_expression(context, node->children[1]);
    if (left == LANG_TYPE_ERROR || right == LANG_TYPE_ERROR)
        return LANG_TYPE_ERROR;

    switch (node->token_type) {
        case TOKEN_OU:
        case TOKEN_E:
        case TOKEN_XOR:
            if (left != LANG_TYPE_BOOL || right != LANG_TYPE_BOOL) {
                semantic_error(context, node,
                               "operador '%s' exige operandos bool, encontrados %s e %s",
                               node->lexeme, lang_type_name(left),
                               lang_type_name(right));
                return LANG_TYPE_ERROR;
            }
            return LANG_TYPE_BOOL;

        case TOKEN_EQUAL_EQUAL:
        case TOKEN_BANG:
            if (left == right ||
                (lang_type_is_numeric(left) && lang_type_is_numeric(right)))
                return LANG_TYPE_BOOL;
            semantic_error(context, node,
                           "comparador '%s' exige tipos compativeis, encontrados %s e %s",
                           node->lexeme, lang_type_name(left),
                           lang_type_name(right));
            return LANG_TYPE_ERROR;

        case TOKEN_LESS:
        case TOKEN_GREATER:
            if (lang_type_is_numeric(left) && lang_type_is_numeric(right))
                return LANG_TYPE_BOOL;
            semantic_error(context, node,
                           "comparador '%s' exige operandos numericos, encontrados %s e %s",
                           node->lexeme, lang_type_name(left),
                           lang_type_name(right));
            return LANG_TYPE_ERROR;

        case TOKEN_INTEGER_DIV:
            if (left == LANG_TYPE_INTEIRA && right == LANG_TYPE_INTEIRA)
                return LANG_TYPE_INTEIRA;
            semantic_error(context, node,
                           "operador '//' exige operandos inteira, encontrados %s e %s",
                           lang_type_name(left), lang_type_name(right));
            return LANG_TYPE_ERROR;

        case TOKEN_SLASH:
            if (!lang_type_is_numeric(left) || !lang_type_is_numeric(right)) {
                semantic_error(context, node,
                               "operador '/' exige operandos numericos, encontrados %s e %s",
                               lang_type_name(left), lang_type_name(right));
                return LANG_TYPE_ERROR;
            }
            common = lang_type_common_numeric(left, right);
            return common == LANG_TYPE_DUPLOCARPADO
                       ? LANG_TYPE_DUPLOCARPADO
                       : LANG_TYPE_FLUT;

        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_CARET:
            common = lang_type_common_numeric(left, right);
            if (common != LANG_TYPE_ERROR) return common;
            semantic_error(context, node,
                           "operador '%s' exige operandos numericos, encontrados %s e %s",
                           node->lexeme, lang_type_name(left),
                           lang_type_name(right));
            return LANG_TYPE_ERROR;

        default:
            semantic_error(context, node,
                           "operador binario '%s' nao possui regra semantica",
                           node->lexeme != NULL ? node->lexeme : "<desconhecido>");
            return LANG_TYPE_ERROR;
    }
}

static LangType evaluate_unary(SemanticContext *context,
                               const AstNode *node) {
    LangType operand;
    if (node == NULL || node->child_count == 0U) return LANG_TYPE_ERROR;
    operand = evaluate_expression(context, node->children[0]);
    if (operand == LANG_TYPE_ERROR) return LANG_TYPE_ERROR;

    if (node->token_type == TOKEN_NAO) {
        if (operand == LANG_TYPE_BOOL) return LANG_TYPE_BOOL;
        semantic_error(context, node,
                       "operador 'NÃO' exige operando bool, encontrado %s",
                       lang_type_name(operand));
        return LANG_TYPE_ERROR;
    }

    if (node->token_type == TOKEN_PLUS || node->token_type == TOKEN_MINUS) {
        if (lang_type_is_numeric(operand)) return operand;
        semantic_error(context, node,
                       "operador unario '%s' exige operando numerico, encontrado %s",
                       node->lexeme, lang_type_name(operand));
        return LANG_TYPE_ERROR;
    }

    semantic_error(context, node, "operador unario desconhecido");
    return LANG_TYPE_ERROR;
}

static LangType evaluate_root(SemanticContext *context,
                              const AstNode *node) {
    LangType argument;
    if (node == NULL || node->child_count == 0U) return LANG_TYPE_ERROR;
    argument = evaluate_expression(context, node->children[0]);
    if (argument == LANG_TYPE_ERROR) return LANG_TYPE_ERROR;
    if (!lang_type_is_numeric(argument)) {
        semantic_error(context, node,
                       "raiz exige argumento numerico, encontrado %s",
                       lang_type_name(argument));
        return LANG_TYPE_ERROR;
    }
    return argument == LANG_TYPE_DUPLOCARPADO ? LANG_TYPE_DUPLOCARPADO
                                               : LANG_TYPE_FLUT;
}

static LangType evaluate_expression(SemanticContext *context,
                                    const AstNode *node) {
    if (node == NULL || context->memory_error) return LANG_TYPE_ERROR;

    switch (node->kind) {
        case AST_INTEGER_LITERAL: return LANG_TYPE_INTEIRA;
        case AST_REAL_LITERAL: return LANG_TYPE_FLUT;
        case AST_STRING_LITERAL: return LANG_TYPE_PALAVRA;
        case AST_BOOL_LITERAL: return LANG_TYPE_BOOL;
        case AST_IDENTIFIER: return evaluate_identifier(context, node);
        case AST_VECTOR_ACCESS: return evaluate_vector_access(context, node);
        case AST_CALL: return evaluate_call(context, node);
        case AST_BINARY_EXPR: return evaluate_binary(context, node);
        case AST_UNARY_EXPR: return evaluate_unary(context, node);
        case AST_ROOT_EXPR: return evaluate_root(context, node);
        default:
            semantic_error(context, node,
                           "no %s nao pode ser usado como expressao",
                           ast_node_kind_name(node->kind));
            return LANG_TYPE_ERROR;
    }
}

static LangType evaluate_lvalue(SemanticContext *context,
                                const AstNode *node) {
    const Symbol *symbol;
    if (node == NULL) return LANG_TYPE_ERROR;

    if (node->kind == AST_VECTOR_ACCESS)
        return evaluate_vector_access(context, node);

    if (node->kind != AST_IDENTIFIER) {
        semantic_error(context, node,
                       "lado esquerdo da atribuicao nao e atribuivel");
        return LANG_TYPE_ERROR;
    }

    symbol = lookup_value_symbol(context, node, node->lexeme);
    if (symbol == NULL) return LANG_TYPE_ERROR;
    if (symbol->kind == SYMBOL_VECTOR) {
        semantic_error(context, node,
                       "vetor '%s' exige indice no lado esquerdo da atribuicao",
                       symbol->name);
        return LANG_TYPE_ERROR;
    }
    if (symbol->kind != SYMBOL_VARIABLE && symbol->kind != SYMBOL_PARAMETER) {
        semantic_error(context, node, "'%s' nao e um destino de atribuicao",
                       symbol->name);
        return LANG_TYPE_ERROR;
    }
    return symbol->type;
}

static void analyze_assignment(SemanticContext *context,
                               const AstNode *node) {
    LangType target;
    LangType value;
    if (node == NULL || node->child_count < 2U) {
        semantic_error(context, node, "atribuicao incompleta na AST");
        return;
    }

    target = evaluate_lvalue(context, node->children[0]);
    value = evaluate_expression(context, node->children[1]);
    if (target == LANG_TYPE_ERROR || value == LANG_TYPE_ERROR) return;

    if (!lang_type_can_assign(target, value))
        semantic_error(context, node,
                       "atribuicao incompativel: destino %s, valor %s",
                       lang_type_name(target), lang_type_name(value));
}

static void analyze_return(SemanticContext *context, const AstNode *node) {
    LangType value_type = LANG_TYPE_VAZIO;
    context->saw_return = 1;

    if (context->current_return_type == LANG_TYPE_ERROR) {
        semantic_error(context, node, "'respost' fora de uma funcao");
        return;
    }

    if (node->child_count > 0U)
        value_type = evaluate_expression(context, node->children[0]);

    if (context->current_return_type == LANG_TYPE_VAZIO) {
        if (node->child_count > 0U && value_type != LANG_TYPE_ERROR)
            semantic_error(context, node,
                           "funcao '%s' tem retorno vazio e nao pode retornar valor %s",
                           context->current_function_name,
                           lang_type_name(value_type));
        return;
    }

    if (node->child_count == 0U) {
        semantic_error(context, node,
                       "funcao '%s' deve retornar valor do tipo %s",
                       context->current_function_name,
                       lang_type_name(context->current_return_type));
        return;
    }

    if (value_type != LANG_TYPE_ERROR &&
        !lang_type_can_assign(context->current_return_type, value_type))
        semantic_error(context, node,
                       "retorno de '%s' espera %s, encontrado %s",
                       context->current_function_name,
                       lang_type_name(context->current_return_type),
                       lang_type_name(value_type));
}

static void require_bool_condition(SemanticContext *context,
                                   const AstNode *condition,
                                   const char *construct_name) {
    LangType type = evaluate_expression(context, condition);
    if (type != LANG_TYPE_ERROR && type != LANG_TYPE_BOOL)
        semantic_error(context, condition,
                       "condicao de %s deve resultar em bool, encontrado %s",
                       construct_name, lang_type_name(type));
}

static void analyze_if(SemanticContext *context, const AstNode *node) {
    size_t i;
    if (node == NULL || node->child_count < 2U) return;

    require_bool_condition(context, node->children[0], "cond");
    analyze_block(context, node->children[1], 1, "cond");

    for (i = 2U; i < node->child_count && !context->memory_error; ++i) {
        const AstNode *branch = node->children[i];
        if (branch == NULL) continue;
        if (branch->kind == AST_ELSE_IF && branch->child_count >= 2U) {
            require_bool_condition(context, branch->children[0],
                                   "casocontrario cond");
            analyze_block(context, branch->children[1], 1,
                          "casocontrario-cond");
        } else if (branch->kind == AST_ELSE && branch->child_count >= 1U) {
            analyze_block(context, branch->children[0], 1,
                          "casocontrario");
        }
    }
}

static void analyze_while(SemanticContext *context, const AstNode *node) {
    if (node == NULL || node->child_count < 2U) return;
    require_bool_condition(context, node->children[0], "durante");
    analyze_block(context, node->children[1], 1, "durante");
}

static void analyze_for(SemanticContext *context, const AstNode *node) {
    if (node == NULL || node->child_count < 4U) return;
    analyze_assignment(context, node->children[0]);
    require_bool_condition(context, node->children[1], "repete");
    analyze_assignment(context, node->children[2]);
    analyze_block(context, node->children[3], 1, "repete");
}

static void analyze_statement(SemanticContext *context, const AstNode *node) {
    if (node == NULL || context->memory_error) return;

    switch (node->kind) {
        case AST_VAR_DECL:
        case AST_VECTOR_DECL:
            analyze_declaration(context, node);
            break;
        case AST_ASSIGNMENT:
            analyze_assignment(context, node);
            break;
        case AST_CALL:
            (void)evaluate_call(context, node);
            break;
        case AST_RETURN:
            analyze_return(context, node);
            break;
        case AST_IF:
            analyze_if(context, node);
            break;
        case AST_WHILE:
            analyze_while(context, node);
            break;
        case AST_FOR:
            analyze_for(context, node);
            break;
        case AST_BLOCK:
            analyze_block(context, node, 1, "bloco");
            break;
        default:
            semantic_error(context, node,
                           "no %s nao e um comando semantico reconhecido",
                           ast_node_kind_name(node->kind));
            break;
    }
}

static void analyze_block(SemanticContext *context, const AstNode *block,
                          int create_scope, const char *scope_name) {
    size_t i;
    int entered = 0;
    if (block == NULL || context->memory_error) return;

    if (create_scope) {
        entered = enter_scope(context, scope_name);
        if (!entered) return;
    }

    for (i = 0U; i < block->child_count && !context->memory_error; ++i)
        analyze_statement(context, block->children[i]);

    if (entered) symbol_table_leave_scope(&context->symbols);
}

static void analyze_record(SemanticContext *context, const AstNode *record) {
    size_t i;
    if (record == NULL || context->memory_error) return;
    if (!enter_scope(context, record->lexeme != NULL ? record->lexeme : "registro"))
        return;

    for (i = 0U; i < record->child_count && !context->memory_error; ++i) {
        const AstNode *field = record->children[i];
        if (field != NULL &&
            (field->kind == AST_VAR_DECL || field->kind == AST_VECTOR_DECL))
            analyze_declaration(context, field);
    }

    symbol_table_leave_scope(&context->symbols);
}

static const AstNode *function_body(const AstNode *function) {
    size_t i;
    if (function == NULL) return NULL;
    for (i = 0U; i < function->child_count; ++i)
        if (function->children[i] != NULL &&
            function->children[i]->kind == AST_BLOCK)
            return function->children[i];
    return NULL;
}

static void analyze_function(SemanticContext *context,
                             const AstNode *function) {
    size_t i;
    LangType previous_return_type;
    const char *previous_function_name;
    int previous_saw_return;
    LangType return_type;
    const AstNode *body;

    if (function == NULL || context->memory_error) return;
    return_type = lang_type_from_token(function->token_type);
    body = function_body(function);

    if (!enter_scope(context,
                     function->lexeme != NULL ? function->lexeme : "funcao"))
        return;

    for (i = 0U; i < function->child_count && !context->memory_error; ++i) {
        const AstNode *parameter = function->children[i];
        LangType parameter_type;
        if (parameter == NULL || parameter->kind != AST_PARAMETER) continue;
        parameter_type = lang_type_from_token(parameter->token_type);
        declare_symbol(context, parameter, parameter->lexeme, SYMBOL_PARAMETER,
                       parameter_type, 0U, NULL, 0U);
    }

    previous_return_type = context->current_return_type;
    previous_function_name = context->current_function_name;
    previous_saw_return = context->saw_return;

    context->current_return_type = return_type;
    context->current_function_name =
        function->lexeme != NULL ? function->lexeme : "<funcao>";
    context->saw_return = 0;

    if (body != NULL)
        analyze_block(context, body, 0, NULL);
    else
        semantic_error(context, function, "funcao '%s' nao possui corpo",
                       context->current_function_name);

    if (!context->memory_error && return_type != LANG_TYPE_VAZIO &&
        !context->saw_return)
        semantic_error(context, function,
                       "funcao '%s' com retorno %s nao possui comando respost",
                       context->current_function_name,
                       lang_type_name(return_type));

    context->current_return_type = previous_return_type;
    context->current_function_name = previous_function_name;
    context->saw_return = previous_saw_return;

    symbol_table_leave_scope(&context->symbols);
}

static void analyze_program(SemanticContext *context, const AstNode *program) {
    size_t i;
    if (program == NULL || program->kind != AST_PROGRAM) {
        semantic_error(context, program,
                       "raiz da AST deve ser um PROGRAM");
        return;
    }

    /*
     * Primeira passagem: registra assinaturas globais de funcoes/principal e
     * nomes de registros. Isso permite recursao e chamadas entre funcoes.
     */
    for (i = 0U; i < program->child_count && !context->memory_error; ++i) {
        const AstNode *node = program->children[i];
        if (node != NULL &&
            (node->kind == AST_FUNCTION || node->kind == AST_PRINCIPAL ||
             node->kind == AST_RECORD))
            predeclare_callable(context, node);
    }

    /* Segunda passagem: variaveis respeitam ordem de declaracao. */
    for (i = 0U; i < program->child_count && !context->memory_error; ++i) {
        const AstNode *node = program->children[i];
        if (node == NULL) continue;
        switch (node->kind) {
            case AST_VAR_DECL:
            case AST_VECTOR_DECL:
                analyze_declaration(context, node);
                break;
            case AST_RECORD:
                analyze_record(context, node);
                break;
            case AST_FUNCTION:
            case AST_PRINCIPAL:
                analyze_function(context, node);
                break;
            default:
                semantic_error(context, node,
                               "no %s invalido no nivel global",
                               ast_node_kind_name(node->kind));
                break;
        }
    }
}

SemanticResult semantic_analyze(const AstNode *ast, FILE *error_stream) {
    SemanticContext context;
    SemanticResult result;

    memset(&context, 0, sizeof(context));
    context.error_stream = error_stream != NULL ? error_stream : stderr;
    context.current_return_type = LANG_TYPE_ERROR;

    if (!symbol_table_init(&context.symbols)) {
        result.status = SEMANTIC_MEMORY_ERROR;
        result.error_count = 0U;
        fprintf(context.error_stream,
                "[ERRO INTERNO] memoria insuficiente para criar a tabela de simbolos.\n");
        return result;
    }

    analyze_program(&context, ast);

    result.error_count = context.error_count;
    if (context.memory_error)
        result.status = SEMANTIC_MEMORY_ERROR;
    else if (context.error_count > 0U)
        result.status = SEMANTIC_HAS_ERRORS;
    else
        result.status = SEMANTIC_OK;

    symbol_table_free(&context.symbols);
    return result;
}

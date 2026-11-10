#include "semantic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Validacoes semanticas que dependem do alvo fisico.
 *
 * Uno / ATmega328P:
 * - RAM total: 2048 B;
 * - reserva conservadora para runtime, pilha de chamadas e temporarios: 256 B;
 * - orcamento estatico usado por globais + frame local maximo: 1792 B;
 * - duplocarpado e rejeitado, preservando a semantica de 64 bits da linguagem.
 */

#define UNO_RAM_TOTAL 2048U
#define UNO_RAM_RESERVE 256U
#define UNO_RAM_BUDGET (UNO_RAM_TOTAL - UNO_RAM_RESERVE)

typedef struct {
    const AstNode *program;
    FILE *err;
    size_t error_count;
} TargetContext;

static void target_error(TargetContext *context,
                         const AstNode *node,
                         const char *format, ...) {
    va_list args;

    ++context->error_count;
    if (node != NULL)
        fprintf(context->err,
                "[ERRO SEMANTICO] linha %d, coluna %d: ",
                node->line, node->column);
    else
        fprintf(context->err, "[ERRO SEMANTICO] ");

    va_start(args, format);
    vfprintf(context->err, format, args);
    va_end(args);
    fputc('\n', context->err);
}

static size_t vector_first_identifier(const AstNode *node) {
    return node != NULL && node->kind == AST_VECTOR_DECL ? 1U : 0U;
}

static size_t vector_size(const AstNode *node) {
    char *end = NULL;
    unsigned long value;

    if (node == NULL || node->kind != AST_VECTOR_DECL ||
        node->child_count == 0U || node->children[0] == NULL ||
        node->children[0]->lexeme == NULL)
        return 0U;

    value = strtoul(node->children[0]->lexeme, &end, 10);
    if (end == node->children[0]->lexeme || *end != '\0')
        return 0U;
    return (size_t)value;
}

static const AstNode *find_record(const AstNode *program,
                                  const char *name) {
    size_t i;
    if (program == NULL || name == NULL) return NULL;

    for (i = 0U; i < program->child_count; ++i) {
        const AstNode *node = program->children[i];
        if (node != NULL && node->kind == AST_RECORD &&
            node->lexeme != NULL && strcmp(node->lexeme, name) == 0)
            return node;
    }
    return NULL;
}

static size_t avr_type_size(const AstNode *program,
                            LangType type,
                            const char *record_name);

static size_t record_size(const AstNode *program, const AstNode *record) {
    size_t total = 0U;
    size_t i;

    if (record == NULL) return 0U;

    for (i = 0U; i < record->child_count; ++i) {
        const AstNode *decl = record->children[i];
        LangType type;
        const char *record_name = NULL;
        size_t count = 0U;
        size_t first;
        size_t j;
        size_t multiplier = 1U;

        if (decl == NULL ||
            (decl->kind != AST_VAR_DECL &&
             decl->kind != AST_VECTOR_DECL))
            continue;

        if (decl->token_type == TOKEN_IDENTIFIER) {
            type = LANG_TYPE_RECORD;
            record_name = decl->lexeme;
        } else {
            type = lang_type_from_token(decl->token_type);
        }

        first = vector_first_identifier(decl);
        for (j = first; j < decl->child_count; ++j)
            if (decl->children[j] != NULL &&
                decl->children[j]->kind == AST_IDENTIFIER)
                ++count;

        if (decl->kind == AST_VECTOR_DECL)
            multiplier = vector_size(decl);

        total += count * multiplier *
                 avr_type_size(program, type, record_name);
    }

    return total;
}

static size_t avr_type_size(const AstNode *program,
                            LangType type,
                            const char *record_name) {
    switch (type) {
        case LANG_TYPE_BOOL: return 1U;
        case LANG_TYPE_INTEIRA: return 4U;
        case LANG_TYPE_FLUT: return 4U;
        case LANG_TYPE_PALAVRA: return 2U;
        case LANG_TYPE_DUPLOCARPADO: return 8U;
        case LANG_TYPE_RECORD:
            return record_size(program, find_record(program, record_name));
        default:
            return 0U;
    }
}

static size_t declaration_size(const AstNode *program,
                               const AstNode *decl) {
    LangType type;
    const char *record_name = NULL;
    size_t count = 0U;
    size_t multiplier = 1U;
    size_t first;
    size_t i;

    if (decl == NULL ||
        (decl->kind != AST_VAR_DECL &&
         decl->kind != AST_VECTOR_DECL))
        return 0U;

    if (decl->token_type == TOKEN_IDENTIFIER) {
        type = LANG_TYPE_RECORD;
        record_name = decl->lexeme;
    } else {
        type = lang_type_from_token(decl->token_type);
    }

    first = vector_first_identifier(decl);
    for (i = first; i < decl->child_count; ++i)
        if (decl->children[i] != NULL &&
            decl->children[i]->kind == AST_IDENTIFIER)
            ++count;

    if (decl->kind == AST_VECTOR_DECL)
        multiplier = vector_size(decl);

    return count * multiplier *
           avr_type_size(program, type, record_name);
}

static size_t local_declarations_size(const AstNode *program,
                                      const AstNode *node) {
    size_t total = 0U;
    size_t i;

    if (node == NULL) return 0U;
    if (node->kind == AST_VAR_DECL || node->kind == AST_VECTOR_DECL)
        return declaration_size(program, node);

    for (i = 0U; i < node->child_count; ++i)
        total += local_declarations_size(program, node->children[i]);

    return total;
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

static size_t parameter_size(const AstNode *program,
                             const AstNode *function) {
    size_t total = 0U;
    size_t i;

    for (i = 0U; i < function->child_count; ++i) {
        const AstNode *param = function->children[i];
        if (param == NULL || param->kind != AST_PARAMETER) continue;
        total += avr_type_size(program,
                               lang_type_from_token(param->token_type),
                               param->lexeme);
    }
    return total;
}

static int contains_duplo(const AstNode *node) {
    size_t i;
    if (node == NULL) return 0;
    if (node->token_type == TOKEN_DUPLOCARPADO ||
        node->inferred_type == LANG_TYPE_DUPLOCARPADO)
        return 1;
    for (i = 0U; i < node->child_count; ++i)
        if (contains_duplo(node->children[i]))
            return 1;
    return 0;
}

SemanticResult semantic_validate_target(const AstNode *ast,
                                        SemanticTarget target,
                                        FILE *error_stream) {
    TargetContext context;
    SemanticResult result;
    size_t globals = 0U;
    size_t i;

    memset(&context, 0, sizeof(context));
    context.program = ast;
    context.err = error_stream != NULL ? error_stream : stderr;

    if (target == SEMANTIC_TARGET_PC) {
        result.status = SEMANTIC_OK;
        result.error_count = 0U;
        return result;
    }

    if (ast == NULL || ast->kind != AST_PROGRAM) {
        target_error(&context, ast,
                     "AST invalida para validacao do alvo uno");
    } else {
        if (contains_duplo(ast))
            target_error(
                &context, ast,
                "o alvo 'uno' nao suporta duplocarpado de 64 bits; use flut ou compile para pc");

        for (i = 0U; i < ast->child_count; ++i) {
            const AstNode *node = ast->children[i];
            if (node != NULL &&
                (node->kind == AST_VAR_DECL ||
                 node->kind == AST_VECTOR_DECL))
                globals += declaration_size(ast, node);
        }

        for (i = 0U; i < ast->child_count; ++i) {
            const AstNode *function = ast->children[i];
            const AstNode *body;
            size_t frame;
            size_t total;

            if (function == NULL ||
                (function->kind != AST_FUNCTION &&
                 function->kind != AST_PRINCIPAL))
                continue;

            body = function_body(function);
            frame = parameter_size(ast, function) +
                    local_declarations_size(ast, body);
            total = globals + frame;

            if (total > UNO_RAM_BUDGET) {
                target_error(
                    &context, function,
                    "a funcao '%s' precisa de aproximadamente %zu bytes de RAM (globais=%zu, frame=%zu); o alvo 'uno' reserva %u de %u bytes para dados e pilha",
                    function->lexeme != NULL
                        ? function->lexeme : "<funcao>",
                    total, globals, frame,
                    (unsigned)UNO_RAM_BUDGET,
                    (unsigned)UNO_RAM_TOTAL);
            }
        }
    }

    result.error_count = context.error_count;
    result.status = context.error_count == 0U
                        ? SEMANTIC_OK
                        : SEMANTIC_HAS_ERRORS;
    return result;
}

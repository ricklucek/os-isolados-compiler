#include "ast.h"

/*
 * Estrutura e utilitarios da AST.
 * Os nos possuem filhos dinamicos; a liberacao usa uma pilha explicita para
 * evitar estouro da pilha C em arvores muito profundas.
 */

#include <stdlib.h>
#include <string.h>

#define AST_INITIAL_CHILD_CAPACITY 4U

static char *copy_string(const char *text) {
    size_t length;
    char *copy;
    if (text == NULL) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length + 1U);
    return copy;
}

AstNode *ast_node_create(AstNodeKind kind, TokenType token_type,
                         const char *lexeme, int line, int column) {
    AstNode *node = calloc(1U, sizeof(*node));
    if (node == NULL) return NULL;

    node->kind = kind;
    node->token_type = token_type;
    node->line = line;
    node->column = column;

    if (lexeme != NULL) {
        node->lexeme = copy_string(lexeme);
        if (node->lexeme == NULL) {
            free(node);
            return NULL;
        }
    }
    return node;
}

AstNode *ast_node_from_token(AstNodeKind kind, const Token *token) {
    if (token == NULL)
        return ast_node_create(kind, TOKEN_INVALID, NULL, 0, 0);
    return ast_node_create(kind, token->type, token->lexeme,
                           token->line, token->column);
}

int ast_node_add_child(AstNode *parent, AstNode *child) {
    AstNode **new_children;
    size_t new_capacity;

    if (parent == NULL || child == NULL) return 0;

    if (parent->child_count == parent->child_capacity) {
        new_capacity = parent->child_capacity == 0U
                           ? AST_INITIAL_CHILD_CAPACITY
                           : parent->child_capacity * 2U;
        new_children = realloc(parent->children,
                               new_capacity * sizeof(*new_children));
        if (new_children == NULL) return 0;
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count++] = child;
    return 1;
}

typedef struct {
    AstNode *node;
    size_t next_child;
} AstFreeFrame;

static void ast_free_recursive_fallback(AstNode *node) {
    size_t i;
    if (node == NULL) return;
    for (i = 0U; i < node->child_count; ++i)
        ast_free_recursive_fallback(node->children[i]);
    free(node->children);
    free(node->lexeme);
    free(node);
}

void ast_free(AstNode *node) {
    AstFreeFrame *stack;
    size_t count = 0U;
    size_t capacity = 64U;

    if (node == NULL) return;

    stack = malloc(capacity * sizeof(*stack));
    if (stack == NULL) {
        ast_free_recursive_fallback(node);
        return;
    }

    stack[count].node = node;
    stack[count].next_child = 0U;
    ++count;

    while (count > 0U) {
        AstFreeFrame *frame = &stack[count - 1U];
        AstNode *current = frame->node;

        if (frame->next_child < current->child_count) {
            AstNode *child = current->children[frame->next_child++];
            AstFreeFrame *grown;

            if (child == NULL) continue;
            if (count == capacity) {
                size_t new_capacity = capacity * 2U;
                grown = realloc(stack, new_capacity * sizeof(*grown));
                if (grown == NULL) {
                    /*
                     * Falha de memoria durante a limpeza: libera apenas esse
                     * ramo recursivamente e continua usando a pilha existente.
                     */
                    ast_free_recursive_fallback(child);
                    continue;
                }
                stack = grown;
                capacity = new_capacity;
                frame = &stack[count - 1U];
                (void)frame;
            }

            stack[count].node = child;
            stack[count].next_child = 0U;
            ++count;
            continue;
        }

        free(current->children);
        free(current->lexeme);
        free(current);
        --count;
    }

    free(stack);
}

const char *ast_node_kind_name(AstNodeKind kind) {
    switch (kind) {
        case AST_PROGRAM: return "PROGRAM";
        case AST_RECORD: return "RECORD";
        case AST_FUNCTION: return "FUNCTION";
        case AST_PRINCIPAL: return "PRINCIPAL";
        case AST_PARAMETER: return "PARAMETER";
        case AST_VAR_DECL: return "VAR_DECL";
        case AST_VECTOR_DECL: return "VECTOR_DECL";
        case AST_BLOCK: return "BLOCK";
        case AST_ASSIGNMENT: return "ASSIGNMENT";
        case AST_RETURN: return "RETURN";
        case AST_IF: return "IF";
        case AST_ELSE_IF: return "ELSE_IF";
        case AST_ELSE: return "ELSE";
        case AST_WHILE: return "WHILE";
        case AST_FOR: return "FOR";
        case AST_CALL: return "CALL";
        case AST_VECTOR_ACCESS: return "VECTOR_ACCESS";
        case AST_FIELD_ACCESS: return "FIELD_ACCESS";
        case AST_IDENTIFIER: return "IDENTIFIER";
        case AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case AST_REAL_LITERAL: return "REAL_LITERAL";
        case AST_STRING_LITERAL: return "STRING_LITERAL";
        case AST_BOOL_LITERAL: return "BOOL_LITERAL";
        case AST_BINARY_EXPR: return "BINARY_EXPR";
        case AST_UNARY_EXPR: return "UNARY_EXPR";
        case AST_ROOT_EXPR: return "ROOT_EXPR";
        default: return "UNKNOWN_AST_NODE";
    }
}

static void ast_print_node(const AstNode *node, FILE *stream,
                           const char *prefix, int last) {
    size_t i;
    char next_prefix[512];
    const char *kind;

    if (node == NULL || stream == NULL) return;

    kind = ast_node_kind_name(node->kind);
    fprintf(stream, "%s%s%s", prefix, last ? "`-- " : "|-- ", kind);

    if (node->lexeme != NULL && node->lexeme[0] != '\0')
        fprintf(stream, " [%s]", node->lexeme);

    if ((node->kind == AST_VAR_DECL || node->kind == AST_VECTOR_DECL ||
         node->kind == AST_PARAMETER || node->kind == AST_FUNCTION ||
         node->kind == AST_PRINCIPAL) &&
        node->token_type != TOKEN_INVALID)
        fprintf(stream, " <%s>", token_type_name(node->token_type));

    fprintf(stream, " @%d:%d\n", node->line, node->column);

    snprintf(next_prefix, sizeof(next_prefix), "%s%s",
             prefix, last ? "    " : "|   ");

    for (i = 0U; i < node->child_count; ++i)
        ast_print_node(node->children[i], stream, next_prefix,
                       i + 1U == node->child_count);
}

void ast_print(const AstNode *node, FILE *stream) {
    size_t i;
    if (node == NULL || stream == NULL) return;
    fprintf(stream, "%s", ast_node_kind_name(node->kind));
    if (node->lexeme != NULL && node->lexeme[0] != '\0')
        fprintf(stream, " [%s]", node->lexeme);
    fprintf(stream, " @%d:%d\n", node->line, node->column);

    for (i = 0U; i < node->child_count; ++i)
        ast_print_node(node->children[i], stream, "",
                       i + 1U == node->child_count);
}

#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>

#include "token.h"

typedef enum {
    AST_PROGRAM = 0,
    AST_RECORD,
    AST_FUNCTION,
    AST_PRINCIPAL,
    AST_PARAMETER,
    AST_VAR_DECL,
    AST_VECTOR_DECL,
    AST_BLOCK,
    AST_ASSIGNMENT,
    AST_RETURN,
    AST_IF,
    AST_ELSE_IF,
    AST_ELSE,
    AST_WHILE,
    AST_FOR,
    AST_CALL,
    AST_VECTOR_ACCESS,
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_REAL_LITERAL,
    AST_BOOL_LITERAL,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_ROOT_EXPR
} AstNodeKind;

typedef struct AstNode {
    AstNodeKind kind;
    TokenType token_type;
    char *lexeme;
    int line;
    int column;
    struct AstNode **children;
    size_t child_count;
    size_t child_capacity;
} AstNode;

AstNode *ast_node_create(AstNodeKind kind, TokenType token_type,
                         const char *lexeme, int line, int column);
AstNode *ast_node_from_token(AstNodeKind kind, const Token *token);
int ast_node_add_child(AstNode *parent, AstNode *child);
void ast_free(AstNode *node);
const char *ast_node_kind_name(AstNodeKind kind);
void ast_print(const AstNode *node, FILE *stream);

#endif

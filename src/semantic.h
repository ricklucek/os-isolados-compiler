#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stddef.h>
#include <stdio.h>

#include "ast.h"

typedef enum {
    SEMANTIC_OK = 0,
    SEMANTIC_HAS_ERRORS = 1,
    SEMANTIC_MEMORY_ERROR = 2
} SemanticStatus;

typedef struct {
    SemanticStatus status;
    size_t error_count;
} SemanticResult;

typedef enum {
    SEMANTIC_TARGET_PC = 0,
    SEMANTIC_TARGET_UNO
} SemanticTarget;

SemanticResult semantic_analyze(const AstNode *ast, FILE *error_stream);
SemanticResult semantic_validate_target(const AstNode *ast,
                                        SemanticTarget target,
                                        FILE *error_stream);

#endif

#ifndef CODEGEN_MLIR_H
#define CODEGEN_MLIR_H

#include <stddef.h>
#include <stdio.h>

#include "ast.h"

typedef enum {
    CODEGEN_MLIR_OK = 0,
    CODEGEN_MLIR_HAS_ERRORS,
    CODEGEN_MLIR_MEMORY_ERROR
} CodegenMlirStatus;

typedef struct {
    CodegenMlirStatus status;
    size_t error_count;
    size_t kernel_count;
} CodegenMlirResult;

/*
 * Emite um modulo MLIR textual contendo somente os nucleos vetoriais
 * encontrados na AST semanticamente anotada. O programa escalar continua
 * pertencendo ao backend LLVM do PjBL 3.
 */
CodegenMlirResult codegen_mlir_generate(const AstNode *ast,
                                        FILE *output,
                                        FILE *error_stream);

#endif

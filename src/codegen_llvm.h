#ifndef CODEGEN_LLVM_H
#define CODEGEN_LLVM_H

#include <stddef.h>
#include <stdio.h>

#include "ast.h"

typedef enum {
    CODEGEN_LLVM_OK = 0,
    CODEGEN_LLVM_HAS_ERRORS,
    CODEGEN_LLVM_MEMORY_ERROR
} CodegenLlvmStatus;

typedef struct {
    CodegenLlvmStatus status;
    size_t error_count;
} CodegenLlvmResult;

/*
 * Emite LLVM IR textual a partir de uma AST que ja passou pela analise
 * semantica. O back-end consome inferred_type; ele nao repete inferencia.
 */
CodegenLlvmResult codegen_llvm_generate(const AstNode *ast,
                                        FILE *output,
                                        FILE *error_stream);

#endif

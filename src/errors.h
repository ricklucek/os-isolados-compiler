#ifndef ERRORS_H
#define ERRORS_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    COMPILER_PHASE_LEXICAL = 0,
    COMPILER_PHASE_SYNTACTIC,
    COMPILER_PHASE_SEMANTIC
} CompilerPhase;

typedef enum {
    COMPILER_EXIT_OK = 0,
    COMPILER_EXIT_USAGE = 1,
    COMPILER_EXIT_LEXICAL_ERROR = 2,
    COMPILER_EXIT_LEXER_INTERNAL = 3,
    COMPILER_EXIT_SYNTACTIC_ERROR = 4,
    COMPILER_EXIT_PARSER_INTERNAL = 5,
    COMPILER_EXIT_SEMANTIC_ERROR = 6,
    COMPILER_EXIT_SEMANTIC_INTERNAL = 7
} CompilerExitCode;

const char *compiler_phase_name(CompilerPhase phase);
const char *compiler_exit_code_name(CompilerExitCode code);
void compiler_print_phase_success(FILE *stream, CompilerPhase phase);
void compiler_print_phase_failure(FILE *stream, CompilerPhase phase,
                                  size_t error_count);
void compiler_print_internal_failure(FILE *stream, CompilerPhase phase,
                                     const char *reason);

#endif

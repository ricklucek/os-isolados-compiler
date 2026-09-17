#include "errors.h"

const char *compiler_phase_name(CompilerPhase phase) {
    switch (phase) {
        case COMPILER_PHASE_LEXICAL: return "lexica";
        case COMPILER_PHASE_SYNTACTIC: return "sintatica";
        case COMPILER_PHASE_SEMANTIC: return "semantica";
        default: return "desconhecida";
    }
}

const char *compiler_exit_code_name(CompilerExitCode code) {
    switch (code) {
        case COMPILER_EXIT_OK: return "sucesso";
        case COMPILER_EXIT_USAGE: return "uso_invalido";
        case COMPILER_EXIT_LEXICAL_ERROR: return "erro_lexico";
        case COMPILER_EXIT_LEXER_INTERNAL: return "falha_lexer";
        case COMPILER_EXIT_SYNTACTIC_ERROR: return "erro_sintatico";
        case COMPILER_EXIT_PARSER_INTERNAL: return "falha_parser";
        case COMPILER_EXIT_SEMANTIC_ERROR: return "erro_semantico";
        case COMPILER_EXIT_SEMANTIC_INTERNAL: return "falha_semantica";
        default: return "codigo_desconhecido";
    }
}

void compiler_print_phase_success(FILE *stream, CompilerPhase phase) {
    if (stream == NULL) return;
    fprintf(stream, "Analise %s: OK.\n", compiler_phase_name(phase));
}

void compiler_print_phase_failure(FILE *stream, CompilerPhase phase,
                                  size_t error_count) {
    if (stream == NULL) return;
    fprintf(stream, "Analise %s concluida com %zu erro(s).\n",
            compiler_phase_name(phase), error_count);
}

void compiler_print_internal_failure(FILE *stream, CompilerPhase phase,
                                     const char *reason) {
    if (stream == NULL) return;
    fprintf(stream, "Analise %s interrompida: %s.\n",
            compiler_phase_name(phase),
            reason != NULL ? reason : "falha interna");
}

#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

static void print_usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Uso: %s <arquivo.mac> [--tokens] [--ast] [--lexer-only] [--parser-only]\n",
            program);
}

static void print_help(const char *program) {
    print_usage(stdout, program);
    printf("\nOpcoes:\n");
    printf("  --tokens       exibe o fluxo de tokens gerado pelo lexer\n");
    printf("  --ast          exibe a AST produzida pelo parser\n");
    printf("  --lexer-only   executa somente a analise lexica\n");
    printf("  --parser-only  executa lexer + parser, sem analise semantica\n");
    printf("  -h, --help     exibe esta ajuda\n");
    printf("\nCodigos de saida:\n");
    printf("  0 sucesso\n");
    printf("  1 uso invalido da linha de comando\n");
    printf("  2 erro lexico\n");
    printf("  3 falha interna/de leitura no lexer\n");
    printf("  4 erro sintatico\n");
    printf("  5 falha interna no parser\n");
    printf("  6 erro semantico\n");
    printf("  7 falha interna no analisador semantico\n");
}

int main(int argc, char *argv[]) {
    const char *source_path;
    int print_tokens = 0;
    int print_ast = 0;
    int lexer_only = 0;
    int parser_only = 0;
    TokenList tokens;
    LexerStatus lexer_status;
    ParserResult parser_result;
    SemanticResult semantic_result;
    AstNode *ast = NULL;
    size_t invalid_count;
    int i;

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help(argv[0]);
        return COMPILER_EXIT_OK;
    }

    if (argc < 2 || argc > 5 || argv[1][0] == '\0') {
        print_usage(stderr, argv[0]);
        return COMPILER_EXIT_USAGE;
    }

    source_path = argv[1];
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--tokens") == 0)
            print_tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0)
            print_ast = 1;
        else if (strcmp(argv[i], "--lexer-only") == 0)
            lexer_only = 1;
        else if (strcmp(argv[i], "--parser-only") == 0)
            parser_only = 1;
        else {
            fprintf(stderr, "Erro: opcao desconhecida '%s'.\n", argv[i]);
            print_usage(stderr, argv[0]);
            return COMPILER_EXIT_USAGE;
        }
    }

    if ((lexer_only && print_ast) || (lexer_only && parser_only)) {
        fprintf(stderr,
                "Erro: --lexer-only nao pode ser combinado com --ast ou --parser-only.\n");
        return COMPILER_EXIT_USAGE;
    }

    token_list_init(&tokens);
    lexer_status = lexer_scan_file(source_path, &tokens, stderr);
    invalid_count = token_list_count_type(&tokens, TOKEN_INVALID);

    if (print_tokens) token_list_print(&tokens, stdout);

    if (lexer_status == LEXER_HAS_ERRORS) {
        compiler_print_phase_failure(stderr, COMPILER_PHASE_LEXICAL,
                                     invalid_count);
        token_list_free(&tokens);
        return COMPILER_EXIT_LEXICAL_ERROR;
    }

    if (lexer_status != LEXER_OK) {
        if (lexer_status == LEXER_MEMORY_ERROR)
            compiler_print_internal_failure(stderr, COMPILER_PHASE_LEXICAL,
                                            "memoria insuficiente");
        token_list_free(&tokens);
        return COMPILER_EXIT_LEXER_INTERNAL;
    }

    if (lexer_only) {
        size_t visible_tokens =
            tokens.count > 0U ? tokens.count - 1U : 0U;
        printf("Analise lexica concluida: %zu token(s), nenhum erro.\n",
               visible_tokens);
        token_list_free(&tokens);
        return COMPILER_EXIT_OK;
    }

    parser_result = parser_parse_ast(&tokens, &ast, stderr);
    if (parser_result.status == PARSER_HAS_ERRORS) {
        compiler_print_phase_failure(stderr, COMPILER_PHASE_SYNTACTIC,
                                     parser_result.error_count);
        token_list_free(&tokens);
        return COMPILER_EXIT_SYNTACTIC_ERROR;
    }

    if (parser_result.status == PARSER_MEMORY_ERROR) {
        compiler_print_internal_failure(stderr, COMPILER_PHASE_SYNTACTIC,
                                        "memoria insuficiente");
        token_list_free(&tokens);
        return COMPILER_EXIT_PARSER_INTERNAL;
    }

    compiler_print_phase_success(stdout, COMPILER_PHASE_LEXICAL);
    compiler_print_phase_success(stdout, COMPILER_PHASE_SYNTACTIC);

    if (print_ast) {
        printf("AST:\n");
        ast_print(ast, stdout);
    }

    if (parser_only) {
        ast_free(ast);
        token_list_free(&tokens);
        return COMPILER_EXIT_OK;
    }

    semantic_result = semantic_analyze(ast, stderr);
    if (semantic_result.status == SEMANTIC_HAS_ERRORS) {
        compiler_print_phase_failure(stderr, COMPILER_PHASE_SEMANTIC,
                                     semantic_result.error_count);
        ast_free(ast);
        token_list_free(&tokens);
        return COMPILER_EXIT_SEMANTIC_ERROR;
    }

    if (semantic_result.status == SEMANTIC_MEMORY_ERROR) {
        compiler_print_internal_failure(stderr, COMPILER_PHASE_SEMANTIC,
                                        "memoria insuficiente");
        ast_free(ast);
        token_list_free(&tokens);
        return COMPILER_EXIT_SEMANTIC_INTERNAL;
    }

    compiler_print_phase_success(stdout, COMPILER_PHASE_SEMANTIC);
    ast_free(ast);
    token_list_free(&tokens);
    return COMPILER_EXIT_OK;
}

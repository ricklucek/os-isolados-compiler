#include <stdio.h>
#include <string.h>

/*
 * Ponto de entrada do front-end. Este arquivo apenas orquestra as fases:
 * fonte -> lexer -> TokenList -> parser/AST -> analise semantica.
 * Uma fase posterior so executa quando a anterior termina com representacao valida.
 */

#include "ast.h"
#include "codegen_llvm.h"
#include "codegen_mlir.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

static void print_usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Uso: %s <arquivo.mcn> [--tokens] [--ast] [--lexer-only] [--parser-only] [-o saida.ll] [--emit-mlir saida.mlir]\n",
            program);
}

static void print_help(const char *program) {
    print_usage(stdout, program);
    printf("\nOpcoes:\n");
    printf("  --tokens       exibe o fluxo de tokens gerado pelo lexer\n");
    printf("  --ast          exibe a AST produzida pelo parser\n");
    printf("  --lexer-only   executa somente a analise lexica\n");
    printf("  --parser-only  executa lexer + parser, sem analise semantica\n");
    printf("  -o <arquivo>   gera LLVM IR textual apos validar o programa\n");
    printf("  --emit-mlir <arquivo> gera modulo MLIR com kernels vetoriais da DSL\n");
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
    printf("  8 construcao valida ainda nao suportada pelo codegen LLVM\n");
    printf("  9 falha interna/de escrita no codegen LLVM\n");
    printf("  10 falha de geracao dos kernels MLIR\n");
    printf("  11 falha interna/de escrita no codegen MLIR\n");
}

int main(int argc, char *argv[]) {
    const char *source_path;
    const char *output_path = NULL;
    const char *mlir_output_path = NULL;
    int print_tokens = 0;
    int print_ast = 0;
    int lexer_only = 0;
    int parser_only = 0;
    TokenList tokens;
    LexerStatus lexer_status;
    ParserResult parser_result;
    SemanticResult semantic_result;
    CodegenLlvmResult codegen_result;
    CodegenMlirResult mlir_result;
    AstNode *ast = NULL;
    size_t invalid_count;
    int i;

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help(argv[0]);
        return COMPILER_EXIT_OK;
    }

    if (argc < 2 || argv[1][0] == '\0') {
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
        else if (strcmp(argv[i], "-o") == 0) {
            if (output_path != NULL || i + 1 >= argc || argv[i + 1][0] == '\0') {
                fprintf(stderr, "Erro: -o exige exatamente um caminho de saida.\n");
                return COMPILER_EXIT_USAGE;
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--emit-mlir") == 0) {
            if (mlir_output_path != NULL || i + 1 >= argc ||
                argv[i + 1][0] == '\0') {
                fprintf(stderr,
                        "Erro: --emit-mlir exige exatamente um caminho de saida.\n");
                return COMPILER_EXIT_USAGE;
            }
            mlir_output_path = argv[++i];
        } else {
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

    if ((output_path != NULL || mlir_output_path != NULL) &&
        (lexer_only || parser_only)) {
        fprintf(stderr,
                "Erro: geracao de codigo exige o pipeline completo ate a analise semantica.\n");
        return COMPILER_EXIT_USAGE;
    }

    if (output_path != NULL && mlir_output_path != NULL) {
        fprintf(stderr,
                "Erro: use -o ou --emit-mlir separadamente neste checkpoint.\n");
        return COMPILER_EXIT_USAGE;
    }

    if (output_path != NULL && strcmp(output_path, source_path) == 0) {
        fprintf(stderr,
                "Erro: arquivo de saida nao pode sobrescrever o fonte.\n");
        return COMPILER_EXIT_USAGE;
    }

    if (mlir_output_path != NULL &&
        strcmp(mlir_output_path, source_path) == 0) {
        fprintf(stderr,
                "Erro: arquivo MLIR nao pode sobrescrever o fonte.\n");
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

    if (parser_only) {
        if (print_ast) {
            printf("AST:\n");
            ast_print(ast, stdout);
        }
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

    if (print_ast) {
        printf("AST anotada:\n");
        ast_print(ast, stdout);
    }

    if (mlir_output_path != NULL) {
        FILE *mlir_output = fopen(mlir_output_path, "wb");
        int close_failed;

        if (mlir_output == NULL) {
            fprintf(stderr,
                    "Erro: nao foi possivel abrir '%s' para escrita.\n",
                    mlir_output_path);
            ast_free(ast);
            token_list_free(&tokens);
            return COMPILER_EXIT_MLIR_INTERNAL;
        }

        mlir_result =
            codegen_mlir_generate(ast, mlir_output, stderr);
        close_failed = fclose(mlir_output) != 0;

        if (mlir_result.status != CODEGEN_MLIR_OK || close_failed) {
            (void)remove(mlir_output_path);
            if (close_failed)
                fprintf(stderr,
                        "Erro: falha ao finalizar o arquivo MLIR '%s'.\n",
                        mlir_output_path);

            ast_free(ast);
            token_list_free(&tokens);
            return mlir_result.status == CODEGEN_MLIR_MEMORY_ERROR ||
                           close_failed
                       ? COMPILER_EXIT_MLIR_INTERNAL
                       : COMPILER_EXIT_MLIR_ERROR;
        }

        printf("Geracao MLIR: OK (%s, %zu kernel(s)).\n",
               mlir_output_path, mlir_result.kernel_count);
    }

    if (output_path != NULL) {
        FILE *llvm_output = fopen(output_path, "wb");
        int close_failed;

        if (llvm_output == NULL) {
            fprintf(stderr,
                    "Erro: nao foi possivel abrir '%s' para escrita.\n",
                    output_path);
            ast_free(ast);
            token_list_free(&tokens);
            return COMPILER_EXIT_CODEGEN_INTERNAL;
        }

        codegen_result =
            codegen_llvm_generate(ast, llvm_output, stderr);
        close_failed = fclose(llvm_output) != 0;

        if (codegen_result.status != CODEGEN_LLVM_OK || close_failed) {
            (void)remove(output_path);
            if (close_failed)
                fprintf(stderr,
                        "Erro: falha ao finalizar o arquivo LLVM '%s'.\n",
                        output_path);

            ast_free(ast);
            token_list_free(&tokens);
            return codegen_result.status == CODEGEN_LLVM_MEMORY_ERROR ||
                           close_failed
                       ? COMPILER_EXIT_CODEGEN_INTERNAL
                       : COMPILER_EXIT_CODEGEN_ERROR;
        }

        printf("Geracao LLVM: OK (%s).\n", output_path);
    }

    ast_free(ast);
    token_list_free(&tokens);
    return COMPILER_EXIT_OK;
}

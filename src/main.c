#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

static void print_usage(const char *program) {
    fprintf(stderr,
            "Uso: %s <arquivo.mac> [--tokens] [--ast] [--lexer-only] [--parser-only]\n",
            program);
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

    if (argc < 2 || argc > 5) {
        print_usage(argv[0]);
        return 1;
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
            print_usage(argv[0]);
            return 1;
        }
    }

    if ((lexer_only && print_ast) || (lexer_only && parser_only)) {
        fprintf(stderr,
                "Erro: --lexer-only nao pode ser combinado com --ast ou --parser-only.\n");
        return 1;
    }

    token_list_init(&tokens);
    lexer_status = lexer_scan_file(source_path, &tokens, stderr);
    invalid_count = token_list_count_type(&tokens, TOKEN_INVALID);

    if (print_tokens) token_list_print(&tokens, stdout);

    if (lexer_status == LEXER_HAS_ERRORS) {
        fprintf(stderr,
                "Analise lexica concluida com %zu token(s) invalido(s).\n",
                invalid_count);
        token_list_free(&tokens);
        return 2;
    }

    if (lexer_status != LEXER_OK) {
        if (lexer_status == LEXER_MEMORY_ERROR)
            fprintf(stderr,
                    "Analise lexica interrompida por falta de memoria.\n");
        token_list_free(&tokens);
        return 3;
    }

    if (lexer_only) {
        size_t visible_tokens =
            tokens.count > 0U ? tokens.count - 1U : 0U;
        printf("Analise lexica concluida: %zu token(s), nenhum erro.\n",
               visible_tokens);
        token_list_free(&tokens);
        return 0;
    }

    parser_result = parser_parse_ast(&tokens, &ast, stderr);
    if (parser_result.status == PARSER_HAS_ERRORS) {
        fprintf(stderr,
                "Analise sintatica concluida com %zu erro(s).\n",
                parser_result.error_count);
        token_list_free(&tokens);
        return 4;
    }

    if (parser_result.status == PARSER_MEMORY_ERROR) {
        fprintf(stderr,
                "Analise sintatica interrompida por falta de memoria.\n");
        token_list_free(&tokens);
        return 5;
    }

    printf("Analise lexica: OK.\n");
    printf("Analise sintatica: OK.\n");

    if (print_ast) {
        printf("AST:\n");
        ast_print(ast, stdout);
    }

    if (parser_only) {
        ast_free(ast);
        token_list_free(&tokens);
        return 0;
    }

    semantic_result = semantic_analyze(ast, stderr);
    if (semantic_result.status == SEMANTIC_HAS_ERRORS) {
        fprintf(stderr,
                "Analise semantica concluida com %zu erro(s).\n",
                semantic_result.error_count);
        ast_free(ast);
        token_list_free(&tokens);
        return 6;
    }

    if (semantic_result.status == SEMANTIC_MEMORY_ERROR) {
        fprintf(stderr,
                "Analise semantica interrompida por falta de memoria.\n");
        ast_free(ast);
        token_list_free(&tokens);
        return 7;
    }

    printf("Analise semantica: OK.\n");
    ast_free(ast);
    token_list_free(&tokens);
    return 0;
}

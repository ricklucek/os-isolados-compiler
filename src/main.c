#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "token.h"

static void print_usage(const char *program) {
    fprintf(stderr, "Uso: %s <arquivo.mac> [--tokens]\n", program);
}

int main(int argc, char *argv[]) {
    const char *source_path;
    int print_tokens = 0;
    TokenList tokens;
    LexerStatus status;
    size_t invalid_count;

    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 3) {
        if (strcmp(argv[2], "--tokens") != 0) {
            print_usage(argv[0]);
            return 1;
        }
        print_tokens = 1;
    }

    source_path = argv[1];
    token_list_init(&tokens);
    status = lexer_scan_file(source_path, &tokens, stderr);
    invalid_count = token_list_count_type(&tokens, TOKEN_INVALID);

    if (print_tokens) token_list_print(&tokens, stdout);

    if (status == LEXER_OK) {
        size_t visible_tokens = tokens.count > 0U ? tokens.count - 1U : 0U;
        printf("Analise lexica concluida: %zu token(s), nenhum erro.\n", visible_tokens);
        token_list_free(&tokens);
        return 0;
    }

    if (status == LEXER_HAS_ERRORS) {
        fprintf(stderr, "Analise lexica concluida com %zu token(s) invalido(s).\n", invalid_count);
        token_list_free(&tokens);
        return 2;
    }

    if (status == LEXER_MEMORY_ERROR)
        fprintf(stderr, "Analise lexica interrompida por falta de memoria.\n");

    token_list_free(&tokens);
    return 3;
}

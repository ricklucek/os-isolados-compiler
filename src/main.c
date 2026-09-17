#include <stdio.h>

#include "lexer.h"
#include "parser.h"
#include "semantic.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo.mac>\n", argv[0]);
        return 1;
    }

    const char *source_path = argv[1];
    printf("os-isolados-compiler\n");
    printf("Arquivo de entrada: %s\n", source_path);
    printf("Checkpoint 0: estrutura inicial pronta.\n");
    printf("Próximos módulos: léxico -> sintático -> semântico.\n");

    return 0;
}

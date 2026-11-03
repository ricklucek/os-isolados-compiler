#include "zl_runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void zl_imprime_int(int32_t valor) {
    printf("%" PRId32, valor);
}

void zl_imprime_bool(int32_t valor) {
    fputs(valor != 0 ? "VER" : "FAL", stdout);
}

void zl_imprime_flut(float valor) {
    printf("%.6f", (double)valor);
}

void zl_imprime_duplo(double valor) {
    printf("%.6f", valor);
}

void zl_imprime_texto(const char *texto) {
    fputs(texto != NULL ? texto : "", stdout);
}

void zl_imprime_nl(void) {
    fputc('\n', stdout);
}

int32_t zl_le_int(void) {
    int32_t valor = 0;
    if (scanf("%" SCNd32, &valor) != 1)
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);
    return valor;
}

void zl_erro_execucao(int32_t codigo, int32_t linha) {
    fprintf(stderr,
            "Erro de execucao Macaronica: codigo=%" PRId32
            ", linha=%" PRId32 "\n",
            codigo, linha);
    exit(100 + (int)codigo);
}

int main(void) {
    return (int)zl_principal();
}

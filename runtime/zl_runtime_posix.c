#include "zl_runtime.h"

#include <inttypes.h>
#include <math.h>
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

int32_t zl_pow_i32(int32_t base, int32_t expoente, int32_t linha) {
    uint32_t resultado = 1U;
    uint32_t fator = (uint32_t)base;
    uint32_t n;

    if (expoente < 0)
        zl_erro_execucao(ZL_ERRO_EXPOENTE_NEGATIVO, linha);

    n = (uint32_t)expoente;
    while (n > 0U) {
        if ((n & 1U) != 0U)
            resultado *= fator;
        n >>= 1U;
        if (n > 0U)
            fator *= fator;
    }

    return (int32_t)resultado;
}

float zl_pow_flut(float base, float expoente) {
    return powf(base, expoente);
}

double zl_pow_duplo(double base, double expoente) {
    return pow(base, expoente);
}

float zl_raiz_flut(float valor, int32_t linha) {
    if (valor < 0.0f)
        zl_erro_execucao(ZL_ERRO_RAIZ_NEGATIVA, linha);
    return sqrtf(valor);
}

double zl_raiz_duplo(double valor, int32_t linha) {
    if (valor < 0.0)
        zl_erro_execucao(ZL_ERRO_RAIZ_NEGATIVA, linha);
    return sqrt(valor);
}

int main(void) {
    return (int)zl_principal();
}

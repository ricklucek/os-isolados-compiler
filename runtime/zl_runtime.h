#ifndef ZL_RUNTIME_H
#define ZL_RUNTIME_H

#include <stdint.h>

/*
 * Interface fixa entre o LLVM IR portavel e a plataforma.
 * O mesmo codigo gerado chama estas funcoes; somente a implementacao muda.
 */
int32_t zl_principal(void);

void zl_imprime_int(int32_t valor);
void zl_imprime_bool(int32_t valor);
void zl_imprime_flut(float valor);
void zl_imprime_duplo(double valor);
void zl_imprime_texto(const char *texto);
void zl_imprime_nl(void);

int32_t zl_le_int(void);

void zl_erro_execucao(int32_t codigo, int32_t linha);

int32_t zl_pow_i32(int32_t base, int32_t expoente, int32_t linha);
float zl_pow_flut(float base, float expoente);
double zl_pow_duplo(double base, double expoente);
float zl_raiz_flut(float valor, int32_t linha);
double zl_raiz_duplo(double valor, int32_t linha);

void zl_pino_modo(int32_t pino, int32_t modo);
void zl_pino_escreve(int32_t pino, int32_t valor);
int32_t zl_pino_le(int32_t pino);
void zl_espera(int32_t milissegundos);

enum {
    ZL_PINO_ENTRADA = 0,
    ZL_PINO_SAIDA = 1,
    ZL_PINO_ENTRADA_PULLUP = 2
};

enum {
    ZL_ERRO_INDICE = 1,
    ZL_ERRO_DIV_ZERO = 2,
    ZL_ERRO_ARITMETICO = 3,
    ZL_ERRO_EXPOENTE_NEGATIVO = 4,
    ZL_ERRO_RAIZ_NEGATIVA = 5,
    ZL_ERRO_DIV_OVERFLOW = 6
};

#endif

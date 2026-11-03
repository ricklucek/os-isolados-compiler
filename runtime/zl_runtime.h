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

enum {
    ZL_ERRO_INDICE = 1,
    ZL_ERRO_DIV_ZERO = 2,
    ZL_ERRO_ARITMETICO = 3
};

#endif

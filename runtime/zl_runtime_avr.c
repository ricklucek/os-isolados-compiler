#include "zl_runtime.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define ZL_BAUD 9600UL
#define ZL_UBRR ((F_CPU / (16UL * ZL_BAUD)) - 1UL)

static void uart_init(void) {
    UBRR0H = (uint8_t)(ZL_UBRR >> 8);
    UBRR0L = (uint8_t)ZL_UBRR;
    UCSR0B = (uint8_t)(_BV(RXEN0) | _BV(TXEN0));
    UCSR0C = (uint8_t)(_BV(UCSZ01) | _BV(UCSZ00));
}

static void uart_putc(char value) {
    while ((UCSR0A & _BV(UDRE0)) == 0U) {}
    UDR0 = (uint8_t)value;
}

static char uart_getc(void) {
    while ((UCSR0A & _BV(RXC0)) == 0U) {}
    return (char)UDR0;
}

static void uart_puts(const char *text) {
    if (text == NULL) return;
    while (*text != '\0') uart_putc(*text++);
}

static void uart_uint32(uint32_t value) {
    char buffer[11];
    uint8_t used = 0U;

    if (value == 0U) {
        uart_putc('0');
        return;
    }

    while (value > 0U && used < sizeof(buffer)) {
        buffer[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (used > 0U) uart_putc(buffer[--used]);
}

void zl_imprime_int(int32_t valor) {
    uint32_t magnitude;

    if (valor < 0) {
        uart_putc('-');
        magnitude = (uint32_t)(-(valor + 1));
        ++magnitude;
    } else {
        magnitude = (uint32_t)valor;
    }
    uart_uint32(magnitude);
}

void zl_imprime_bool(int32_t valor) {
    uart_puts(valor != 0 ? "VER" : "FAL");
}

void zl_imprime_flut(float valor) {
    char buffer[24];
    dtostrf(valor, 0, 6, buffer);
    uart_puts(buffer);
}

/*
 * --alvo=uno rejeita duplocarpado. Esta funcao existe somente para manter a
 * interface C completa; nao deve ser referenciada por IR validada para Uno.
 */
void zl_imprime_duplo(double valor) {
    zl_imprime_flut((float)valor);
}

void zl_imprime_texto(const char *texto) {
    uart_puts(texto);
}

void zl_imprime_nl(void) {
    uart_putc('\r');
    uart_putc('\n');
}

int32_t zl_le_int(void) {
    int32_t value = 0;
    int negative = 0;
    char c;

    do {
        c = uart_getc();
    } while (c == '\r' || c == '\n' || c == ' ');

    if (c == '-') {
        negative = 1;
        c = uart_getc();
    }

    while (c >= '0' && c <= '9') {
        value = value * 10 + (int32_t)(c - '0');
        c = uart_getc();
    }

    return negative ? -value : value;
}

static int pin_regs(int32_t pino,
                    volatile uint8_t **ddr,
                    volatile uint8_t **port,
                    volatile uint8_t **pin,
                    uint8_t *bit) {
    if (pino >= 0 && pino <= 7) {
        *ddr = &DDRD; *port = &PORTD; *pin = &PIND;
        *bit = (uint8_t)pino;
        return 1;
    }
    if (pino >= 8 && pino <= 13) {
        *ddr = &DDRB; *port = &PORTB; *pin = &PINB;
        *bit = (uint8_t)(pino - 8);
        return 1;
    }
    if (pino >= 14 && pino <= 19) {
        *ddr = &DDRC; *port = &PORTC; *pin = &PINC;
        *bit = (uint8_t)(pino - 14);
        return 1;
    }
    return 0;
}

void zl_pino_modo(int32_t pino, int32_t modo) {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;
    uint8_t bit;

    if (!pin_regs(pino, &ddr, &port, &pin, &bit))
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);

    if (modo == ZL_PINO_SAIDA) {
        *ddr |= _BV(bit);
        *port &= (uint8_t)~_BV(bit);
    } else if (modo == ZL_PINO_ENTRADA) {
        *ddr &= (uint8_t)~_BV(bit);
        *port &= (uint8_t)~_BV(bit);
    } else if (modo == ZL_PINO_ENTRADA_PULLUP) {
        *ddr &= (uint8_t)~_BV(bit);
        *port |= _BV(bit);
    } else {
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);
    }
    (void)pin;
}

void zl_pino_escreve(int32_t pino, int32_t valor) {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;
    uint8_t bit;

    if (!pin_regs(pino, &ddr, &port, &pin, &bit))
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);

    if (valor != 0)
        *port |= _BV(bit);
    else
        *port &= (uint8_t)~_BV(bit);
    (void)ddr;
    (void)pin;
}

int32_t zl_pino_le(int32_t pino) {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;
    uint8_t bit;

    if (!pin_regs(pino, &ddr, &port, &pin, &bit))
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);

    (void)ddr;
    (void)port;
    return ((*pin & _BV(bit)) != 0U) ? 1 : 0;
}

void zl_espera(int32_t milissegundos) {
    if (milissegundos < 0)
        zl_erro_execucao(ZL_ERRO_ARITMETICO, 0);

    while (milissegundos-- > 0)
        _delay_ms(1.0);
}

void *zl_vetor_endereco(void *base, int32_t indice, int32_t tamanho_elemento) {
    uint8_t *bytes = (uint8_t *)base;
    uint16_t deslocamento =
        (uint16_t)indice * (uint16_t)tamanho_elemento;
    return (void *)(bytes + deslocamento);
}

void zl_erro_execucao(int32_t codigo, int32_t linha) {
    int32_t i;
    int32_t flashes = codigo > 0 && codigo < 10 ? codigo : 1;

    uart_puts("Erro de execucao Macaronica: codigo=");
    zl_imprime_int(codigo);
    uart_puts(", linha=");
    zl_imprime_int(linha);
    zl_imprime_nl();

    zl_pino_modo(13, ZL_PINO_SAIDA);
    for (;;) {
        for (i = 0; i < flashes; ++i) {
            zl_pino_escreve(13, 1);
            zl_espera(150);
            zl_pino_escreve(13, 0);
            zl_espera(150);
        }
        zl_espera(700);
    }
}

int32_t zl_pow_i32(int32_t base, int32_t expoente, int32_t linha) {
    uint32_t result = 1U;
    uint32_t factor = (uint32_t)base;
    uint32_t n;

    if (expoente < 0)
        zl_erro_execucao(ZL_ERRO_EXPOENTE_NEGATIVO, linha);

    n = (uint32_t)expoente;
    while (n > 0U) {
        if ((n & 1U) != 0U) result *= factor;
        n >>= 1U;
        if (n > 0U) factor *= factor;
    }
    return (int32_t)result;
}

float zl_pow_flut(float base, float expoente) {
    return powf(base, expoente);
}

double zl_pow_duplo(double base, double expoente) {
    return (double)powf((float)base, (float)expoente);
}

float zl_raiz_flut(float valor, int32_t linha) {
    if (valor < 0.0f)
        zl_erro_execucao(ZL_ERRO_RAIZ_NEGATIVA, linha);
    return sqrtf(valor);
}

double zl_raiz_duplo(double valor, int32_t linha) {
    return (double)zl_raiz_flut((float)valor, linha);
}

int main(void) {
    uart_init();
    (void)zl_principal();
    for (;;) {}
}

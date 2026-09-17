# Roteiro de Estudo e Defesa

Este documento será ampliado a cada checkpoint. O objetivo é garantir que Henrique, João e Richard consigam explicar qualquer parte do código.

## 1. Visão geral

Todos devem conseguir desenhar e explicar:

```text
Fonte -> Lexer -> Tokens -> Parser -> AST -> Semântico -> Resultado
```

## 2. Perguntas-base

### Léxico
- O que é um lexema?
- O que é um token?
- Como o lexer diferencia palavra reservada de identificador?
- Como linha e coluna são rastreadas?

### Sintático
- O que é uma gramática livre de contexto?
- Como uma produção da gramática vira código no parser?
- Como precedência de operadores será tratada?
- Qual é a função da AST?

### Semântico
- Por que um programa pode ser sintaticamente válido e semanticamente inválido?
- O que é uma tabela de símbolos?
- Como funcionam escopos?
- Como são validados tipos e assinaturas de função?

## 3. Regra de trabalho do grupo

Cada integrante pode liderar partes diferentes, mas nenhum módulo será considerado concluído antes de os três conseguirem explicar seu fluxo principal.

# Auditoria do PjBL 3

Este documento relaciona os requisitos executáveis do PjBL 3 com evidências versionadas no repositório.

## Bateria obrigatória

O enunciado exige no mínimo oito programas executáveis com saída esperada. A bateria atual excede esse mínimo.

| Requisito | Evidência principal |
|---|---|
| Aritmética, precedência e associatividade | `tests/execucao/19_aritmetica_precedencia.mcn` |
| Condicionais aninhadas | `tests/execucao/20_condicional_aninhada.mcn` |
| `durante` e `repete` | `tests/execucao/05_lacos.mcn` |
| Função recursiva | `tests/execucao/09_recursao_forward.mcn` |
| Vetor | `tests/execucao/15_vetores.mcn` |
| Registro | `tests/execucao/16_registro.mcn` |
| Entrada e saída | `tests/execucao/21_entrada_saida.mcn` + `.entrada` |
| Tipos mistos inteira/flut/duplocarpado | `tests/execucao/23_tipos_mistos.mcn` |
| `^` e `raiz()` | `tests/execucao/11_potencia_raiz.mcn` |
| Diferença entre `/` e `//` | `tests/execucao/22_divisao_real_inteira.mcn` |

## Casos de borda

| Caso | Política | Evidência |
|---|---|---|
| Índice dinâmico fora do vetor | erro runtime 1 + linha | `18_erro_indice_dinamico.mcn` |
| Divisão inteira dinâmica por zero | erro runtime 2 + linha | `24_div_zero_dinamica_inteira.mcn` |
| Divisão real dinâmica por zero | erro runtime 2 + linha | `25_div_zero_dinamica_real.mcn` |
| `INT32_MIN // -1` | erro runtime 6 + linha | `26_sdiv_overflow.mcn` |
| `INT32_MIN` válido | aceita e imprime | `27_int32_minimo.mcn` |
| Literal acima de `INT32_MAX` | erro de codegen, sem .ll | `28_inteira_fora_i32.mcn` |
| Divisão por zero literal | erro de codegen, sem .ll | `29_div_zero_constante.mcn` |

## Ferramentas LLVM

A suíte executa:

- `llvm-as-18`;
- `opt-18 -passes=verify`;
- `lli-18`;
- `clang-18` + runtime POSIX;
- `opt-18 -passes=mem2reg` com verificação de `phi`;
- auditoria estática da portabilidade da IR;
- `llc-18` para AVR/ATmega328P em smoke test.

## Portabilidade

`make audit-llvm FILE=...` verifica invariantes do projeto: sem alvo embutido, sem chamadas variádicas/libc no IR, GEP com índices `i32` e funções `nounwind`.

`make check-avr FILE=...` demonstra que a mesma IR independente de target consegue ser selecionada posteriormente para AVR. A etapa ainda não liga o objeto, pois o runtime AVR pertence ao PjBL 4 Parte D.

## Observação sobre os programas de nível do PjBL 1

Os programas históricos `tests/niveis/` continuam sendo testes do front-end e permanecem aceitos ali. Alguns contêm literais maiores que `i32` (por exemplo, identificadores numéricos longos). Como o contrato do backend do PjBL 3 fixa `inteira = i32`, esses valores não são silenciosamente truncados: uma tentativa de gerar LLVM para eles produz diagnóstico de representabilidade e não deixa `.ll` parcial.

# Decisões de Projeto — Macaronica

Este documento registra decisões tomadas pelo grupo para resolver lacunas e ambiguidades da especificação.

## D01 — Linguagem implementada

A disciplina é Linguagens Formais e Compiladores; portanto, o grupo implementará a linguagem **Macaronica**.

## D02 — Acesso a vetor

**Status:** proposta inicial, deve ser validada pelo grupo antes do parser.

A especificação define `tipo vet{tamanho} nome`, mas não define explicitamente a sintaxe de acesso. A proposta inicial é `nome{indice}`.

## D03 — Chamada de função

**Status:** proposta inicial.

A chamada seguirá `nome(argumento1, argumento2, ...)`, conforme a convenção natural indicada no enunciado.

## D04 — Retorno em função `vazio`

**Status:** proposta inicial.

Funções `vazio` poderão omitir `respost`. Caso utilizado, será aceito `respost;`, nunca `respost valor;`.

## D05 — Localização de erros

**Status:** consolidada no Checkpoint 1.

Cada token guarda linha e coluna inicial. O lexer mantém essas posições durante a varredura e as mensagens de erro léxico indicam ambas.

## D06 — AST

O parser produzirá uma AST para separar análise sintática de análise semântica e facilitar inspeção, testes e explicação do projeto.

## D07 — Palavras estruturais ausentes da lista de reservadas

**Status:** consolidada no Checkpoint 1.

`registro`, `outrafuncao`, `edai` e `vet` aparecem nas formas sintáticas, embora não estejam na lista explícita de palavras reservadas.

**Decisão:** o lexer reconhece os quatro como palavras estruturais específicas.

## D08 — Símbolos usados nas estruturas, mas ausentes do alfabeto

**Status:** parcialmente consolidada no Checkpoint 1.

`@` e `,` não aparecem no alfabeto listado, mas aparecem nas estruturas e exemplos.

**Decisão:** ambos são reconhecidos como tokens. Aspas para literais de `palavra` continuam pendentes.

## D09 — Operador lógico `NÃO` e alfabeto ASCII

**Status:** consolidada no Checkpoint 1.

**Decisão:** aceitar literalmente `NÃO` em UTF-8 e não criar o alias `NAO`, preservando o lexema fornecido pela especificação.

## D10 — Entrada e saída na Macaronica

**Status:** pendente de decisão.

Os casos de teste obrigatórios pedem uso de entrada/saída, mas a seção da Macaronica não fornece uma construção de leitura ou impressão. A sintaxe não será inventada sem registro; o grupo deverá consolidar uma convenção ou confirmar com o professor antes de fechar a gramática.

## D11 — Literal do tipo `palavra`

**Status:** pendente de decisão.

O tipo `palavra` é definido, mas a especificação não formaliza a sintaxe de literal textual e aspas não aparecem no alfabeto. O Checkpoint 1 não reconhece literal textual.

## D12 — Literais de `flut` e `duplocarpado`

**Status:** consolidada no Checkpoint 1.

**Decisão:** adotar `DIGITO+ '.' DIGITO+` para literais reais. O ponto somente é aceito dentro desse padrão e não existe como token isolado.

## D13 — Identificadores

**Status:** consolidada no Checkpoint 1.

Identificadores seguem `LETRA (LETRA | DIGITO)*`, usando apenas `a..z`, `A..Z` e `0..9`. `_` não é aceito porque não consta no alfabeto fornecido.

## D14 — Comentários

**Status:** pendente.

A especificação não define comentários. O lexer não interpreta `//` como comentário porque esse lexema é explicitamente definido como operador de divisão inteira.

## Novas ambiguidades encontradas

Toda nova lacuna deve ser registrada com descrição, decisão, justificativa e impacto na gramática ou semântica.

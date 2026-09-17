# Decisões de Projeto — Macaronica

Este documento registra decisões tomadas pelo grupo para resolver lacunas e ambiguidades da especificação.

## D01 — Linguagem implementada

A disciplina é Linguagens Formais e Compiladores; portanto, o grupo implementará a linguagem **Macaronica**.

## D02 — Acesso a vetor

**Status:** proposta inicial, deve ser validada pelo grupo antes do parser.

A especificação define `tipo vet{tamanho} nome`, mas não define explicitamente a sintaxe de acesso. A proposta inicial é:

```text
nome{indice}
```

Justificativa: reutiliza o delimitador já associado a vetores na linguagem e mantém a sintaxe distinguível de chamada de função.

## D03 — Chamada de função

**Status:** proposta inicial.

A chamada de função seguirá:

```text
nome(argumento1, argumento2, ...)
```

Justificativa: é a convenção natural indicada pelo próprio enunciado.

## D04 — Retorno em função `vazio`

**Status:** proposta inicial.

Funções de retorno `vazio` poderão omitir `respost`. Caso seja utilizado, será aceito apenas:

```text
respost;
```

Nunca `respost valor;`.

## D05 — Localização de erros

Tokens guardarão, no mínimo, linha e coluna para permitir mensagens de erro precisas.

## D06 — AST

O parser produzirá uma AST para separar a análise sintática da análise semântica e facilitar inspeção, testes e explicação do projeto.

## D07 — Palavras estruturais ausentes da lista de reservadas

**Status:** pendente de consolidação no Checkpoint 1/2.

A lista de palavras reservadas fornecida pela especificação não contém `registro`, `outrafuncao` e `edai`, embora essas palavras apareçam nas formas sintáticas das estruturas da linguagem.

Decisão provisória: o lexer deverá reconhecê-las como palavras estruturais da Macaronica, pois sem isso as construções apresentadas no próprio enunciado não poderiam ser analisadas. A diferença será explicitamente mantida na documentação como inconsistência da especificação.

## D08 — Símbolos usados nas estruturas, mas ausentes do alfabeto

**Status:** pendente de consolidação no Checkpoint 1.

O alfabeto listado não inclui `@` nem `,`, mas ambos aparecem nas estruturas e exemplos da linguagem. Também não define aspas para literais de `palavra`.

Decisão provisória: `@` e `,` serão reconhecidos porque são exigidos pelas estruturas fornecidas. A sintaxe de literal textual será tratada como lacuna separada e não será assumida silenciosamente.

## D09 — Operador lógico `NÃO` e alfabeto ASCII

**Status:** pendente de consolidação no Checkpoint 1.

A tabela de operadores usa `NÃO`, mas o alfabeto apresentado contém apenas `a..z` e `A..Z`, criando uma inconsistência em relação ao caractere acentuado.

Antes de fechar a regra léxica, o grupo deve escolher entre aceitar literalmente `NÃO`, definir uma forma ASCII (`NAO`) ou aceitar ambas, registrando a escolha e sua justificativa.

## D10 — Entrada e saída na Macaronica

**Status:** pendente de decisão.

Os casos de teste obrigatórios pedem ao menos um uso de entrada/saída, mas a seção específica da Macaronica não fornece uma construção de leitura ou impressão.

A sintaxe não será inventada sem registro. O grupo deve definir uma convenção razoável ou confirmar com o professor antes de fechar a gramática e os testes obrigatórios.

## D11 — Literal do tipo `palavra`

**Status:** pendente de decisão.

O tipo `palavra` é definido, mas a especificação apresentada não formaliza a sintaxe de um literal textual. Essa lacuna afeta o lexer, a gramática e a verificação de tipos e será resolvida explicitamente antes da consolidação dessas fases.

## Novas ambiguidades encontradas

Registrar aqui qualquer lacuna adicional identificada durante a implementação, sempre contendo:

- descrição;
- decisão;
- justificativa;
- impacto na gramática ou semântica.

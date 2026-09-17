# Gramática — Macaronica

> Documento de trabalho. A gramática formal será consolidada antes da implementação do parser.

## Elementos conhecidos da especificação

Tipos:

```text
bool | inteira | flut | palavra | duplocarpado | vazio
```

Estruturas que a gramática deverá contemplar:

- registro;
- função não-principal;
- função principal;
- declaração de variável;
- declaração de vetor;
- atribuição;
- repetição `repete`;
- repetição `durante`;
- decisão `cond` / `casocontrario`;
- retorno `respost`;
- expressões e chamadas de função.

## Condição definida na especificação

A especificação fornece três formas explícitas:

```text
CONDIÇÃO -> VAR COMPARADOR VAR
CONDIÇÃO -> VAR COMPARADOR NUM
CONDIÇÃO -> NUM COMPARADOR NUM
```

## Próxima etapa

No Checkpoint 2, este documento será transformado em uma gramática BNF/EBNF completa e diretamente mapeável para as funções do parser.

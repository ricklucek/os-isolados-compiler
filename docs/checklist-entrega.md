# Checkpoint 6 — Checklist de Entrega

Este documento organiza a entrega contra os requisitos do enunciado, sem substituir a especificação original.

## Estrutura obrigatória

- `src/`: código C modularizado;
- `tests/validos/` e `tests/invalidos/`: suítes por fase;
- `tests/entrega/`: conjunto mínimo organizado pelos requisitos obrigatórios;
- `docs/decisoes.md`: ambiguidades e decisões;
- `Makefile`: compilação por `make`;
- `README.md`: execução, testes, Docker e declaração de uso de IA.

## Casos válidos obrigatórios

O conjunto `tests/entrega/validos/` contém cinco programas. Em conjunto, eles cobrem declaração de variável, vetor, registro, função não-principal, `principal`, `cond`, `casocontrario`, `repete`, `durante`, entrada, saída e literal de `palavra`.

## Casos inválidos obrigatórios

`tests/entrega/invalidos/` contém as cinco classes mínimas:

1. token inválido — saída 2;
2. estrutura sintática malformada — saída 4;
3. variável usada sem declaração — saída 6;
4. incompatibilidade de tipo — saída 6;
5. chamada com assinatura incorreta — saída 6.

`make test-delivery` valida os códigos de saída esperados.

## Robustez

Além do conjunto mínimo, `make test-robustness` cobre múltiplos diagnósticos, arquivos adversos, identificadores longos, crescimento da tabela de símbolos e aninhamento profundo.

## Ambiguidades consolidadas

As lacunas e decisões estão em `docs/decisoes.md`. Para o fechamento da entrega foram resolvidas explicitamente acesso a vetor, chamada de função, retorno `vazio`, palavras estruturais ausentes da lista, símbolos ausentes do alfabeto, `NÃO`, literal `palavra`, entrada/saída, tipos numéricos e retornos.

Comentários permanecem sem implementação porque a especificação não fornece sintaxe e `//` já é divisão inteira.

## Limitações documentadas

- não existe geração de código nem execução do programa-fonte;
- `entrada` e `saida` são validadas, não executadas;
- não há sintaxe de comentários;
- não há sintaxe de escapes em `palavra`;
- a análise de retorno não prova retorno em todos os caminhos.

## Comandos finais

```bash
make clean
make
make test
make test-sanitize
```

No Docker:

```bash
make docker-test
make docker-sanitize
```

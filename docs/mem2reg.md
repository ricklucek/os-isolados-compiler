# mem2reg e instruções PHI

## Por que o gerador começa com memória?

A estratégia do back-end segue a usada pelo Clang em geração não otimizada: cada variável local recebe `alloca` no bloco `entry`; leituras usam `load` e atribuições usam `store`.

Isso simplifica o front-end. A construção SSA das variáveis fica para:

```bash
opt-18 -S -passes=mem2reg entrada.ll -o saida.ll
```

## Exemplo com laço

O teste `tests/execucao/05_lacos.mcn` modifica `i` e `soma` ao longo de `durante` e `repete`.

Antes do `mem2reg`, a IR contém padrões como:

```llvm
%i.addr = alloca i32
store i32 0, ptr %i.addr
%t0 = load i32, ptr %i.addr
```

Depois da promoção, a variável que chega ao cabeçalho do laço pode vir de dois predecessores: a entrada inicial ou a iteração anterior. O LLVM representa essa junção com `phi`:

```llvm
%i.0 = phi i32 [ 0, %entry ], [ %i.prox, %while.body ]
```

Os nomes exatos dependem do `opt`, mas a ideia é estável: o `phi` escolhe o valor conforme o bloco predecessor.

## Como reproduzir

```bash
make show-mem2reg FILE=tests/execucao/05_lacos.mcn
```

Arquivos gerados:

- `.llvm-before.ll`: forma em memória emitida pelo nosso compilador;
- `.llvm-after.ll`: forma promovida por `mem2reg`.

O alvo imprime também as linhas contendo `phi`.

## PHI do curto-circuito

Há um segundo uso de `phi` no projeto. Para `a E b` ou `a OU b`, o gerador produz diretamente `phi i1` no merge do curto-circuito. Esse caso representa o resultado de uma **expressão** e não a promoção de uma variável mutável. Portanto ele não contradiz a decisão de deixar `mem2reg` construir a SSA das variáveis.

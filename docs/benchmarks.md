# Plano experimental — PjBL 4 Parte C

## Objetivo

Comparar desempenho e transformação de IR de pelo menos dois núcleos da DSL vetorial em quatro configurações, mantendo compilação e execução como grandezas separadas.

Os tempos produzidos pela CI **não** serão usados como resultados do relatório, porque runners compartilhados não oferecem ambiente estável para benchmark.

## Núcleos

A campanha final utilizará pelo menos:

1. operação elemento a elemento, inicialmente soma;
2. redução (`somavet`) ou produto escalar (`prodesc`).

A seleção final deve incluir ao menos uma redução/produto escalar, como exige o enunciado.

## Tamanhos

Usar tamanhos suficientemente grandes para amortizar overhead, com referência inicial:

```text
N = 1.000.000
N = 10.000.000
```

Se memória/tempo da máquina tornar 10 milhões impraticável, qualquer alteração deve ser registrada no relatório e aplicada igualmente às configurações comparadas.

## Configurações

### C0 — linha de base

- MLIR sem vetorizar/paralelizar;
- lowering normal até LLVM IR;
- LLVM final sem otimização agressiva (`-O0`).

Objetivo: tempo de referência.

### C1 — otimização genérica LLVM

- mesmo lowering de C0;
- `opt -O2` ou `clang -O2` com target da máquina de benchmark;
- coletar `-Rpass=loop-vectorize` e `-Rpass-missed=loop-vectorize` quando aplicável;
- procurar tipos SIMD (`<4 x ...>`, `<8 x ...>`) na IR.

Objetivo: observar o que o autovetorizador LLVM consegue sozinho.

### C2 — vetorização explícita MLIR

Usar `affine-super-vectorize` no núcleo elemento a elemento. Tamanho virtual inicial:

```text
virtual-vector-size=8
```

Para reduções, testar explicitamente `vectorize-reductions=true` quando suportado pelo pipeline LLVM/MLIR 18 utilizado.

Objetivo: comparar a decisão explícita do MLIR com C1.

### C3 — paralelismo

Transformar loops elegíveis em paralelo e rebaixar `scf.parallel` para OpenMP, seguido de canonicalização conforme exigido pela toolchain MLIR 18.

Executar com diferentes números de threads:

```text
1, 2, 4, ... até o limite físico/lógico útil da máquina
```

Registrar `OMP_NUM_THREADS` em cada execução.

## Metodologia

Para cada combinação núcleo × tamanho × configuração:

1. limpar artefatos da execução anterior;
2. medir tempo de compilação separadamente;
3. executar o binário pelo menos 5 vezes;
4. descartar qualquer warm-up apenas se isso for decidido previamente e documentado;
5. armazenar todas as amostras;
6. calcular mediana;
7. validar resultado numérico antes de aceitar a amostra.

Os resultados serão gravados em CSV e os gráficos serão produzidos por script versionado no próximo checkpoint.

## Ponto flutuante e associatividade

Reduções de ponto flutuante não podem ser reassociadas livremente sob a semântica IEEE estrita. Isso impede ou limita várias transformações vetoriais porque mudar a árvore de somas pode alterar arredondamentos.

A campanha deverá comparar a redução estrita com uma variante que permita reassociação/fast-math e explicar:

- alteração semântica autorizada;
- impacto na vetorização;
- eventual diferença numérica;
- evidência na IR e nos relatórios do compilador.

## Ambiente

Antes da campanha:

```bash
make bench-info > resultados/maquina.txt
```

O arquivo deve acompanhar tabelas/gráficos do relatório.

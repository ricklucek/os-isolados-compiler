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


## Implementação do Checkpoint 17

### Fontes geradas a partir de Macaronica

Os arquivos em `benchmarks/templates/` são fontes Macaronica parametrizadas por `@N@`. O runner gera uma fonte temporária para cada tamanho e chama o compilador do grupo com `--emit-mlir`. Nenhum kernel de benchmark é escrito manualmente em MLIR.

Templates:

- `elementwise.mcn.in`;
- `prodesc.mcn.in`;
- `sumfloat.mcn.in` para o experimento de associatividade.

### C0

`scripts/benchmark_variant.sh C0` executa canonicalização e o lowering normal até LLVM IR. O kernel é compilado com `-O0`.

### C1

Usa o mesmo LLVM IR de C0, mas o kernel é compilado com:

```text
-O2 -march=native
-Rpass=loop-vectorize
-Rpass-missed=loop-vectorize
-Rpass-analysis=loop-vectorize
```

O diretório da variante guarda `rpass.txt` e `kernel_optimized.ll`.

### C2

Antes do lowering:

```text
--affine-super-vectorize="virtual-vector-size=8 vectorize-reductions=true"
```

A evidência intermediária fica em `01_vector.mlir`.

Observação importante: os kernels elemento a elemento são `affine.for` e são candidatos diretos. As reduções atuais são emitidas como `scf.for`; portanto o passe affine pode não transformá-las. Essa ausência é registrada em vez de ser escondida e deve entrar na análise do relatório.

### C3

Aplica:

```text
--affine-parallelize="max-nested=1"
--lower-affine
--convert-scf-to-openmp
--canonicalize
--convert-openmp-to-llvm
```

A evidência `01_parallel.mlir` permite verificar se surgiu `affine.parallel`. Kernels com dependência de redução podem permanecer sequenciais.

A compilação final usa `-O2 -march=native -fopenmp`. O mesmo executável é medido com `OMP_NUM_THREADS=1,2,4,...`.

### Harness e medição

O harness C é gerado automaticamente apenas para alimentar os kernels e cronometrá-los. Os kernels medidos continuam vindo da fonte Macaronica.

Cada amostra mede o tempo médio de uma chamada ao kernel, usando `CLOCK_MONOTONIC`. O número interno de chamadas é escolhido para processar aproximadamente 50 milhões de elementos por amostra, salvo quando `--inner` é fornecido.

Cada repetição recompila a variante e depois executa o binário, portanto há amostras independentes de:

- `compile_ms`;
- `run_ms`;
- checksum numérico.

### Dados e gráficos

`scripts/benchmark.py` grava todas as amostras em `raw.csv` e medianas em `summary.csv`.

`scripts/plot_benchmarks.py` gera SVGs de:

- tempo de execução;
- tempo de compilação;
- speedup de C3 em relação a C0.

### Experimento floating-point

A fonte `sumfloat.mcn.in` usa `somavet` sobre `flut`.

São comparadas:

- **C1**: `fadd` estrito;
- **C1F**: o LLVM IR derivado da mesma fonte é marcado experimentalmente com `fast` em `fadd/fmul`.

C1F não é uma quinta configuração obrigatória; é uma variante controlada para responder à pergunta sobre associatividade/reordenação. O runner registra o checksum de ambas para permitir comparar desempenho e diferença numérica.

### Execução oficial

No ambiente da máquina escolhida:

```bash
make bench-info > resultados/benchmarks/maquina_previa.txt
make bench
```

Não executar outras cargas pesadas em paralelo. Os arquivos de `resultados/` são ignorados pelo Git por padrão; para a entrega final, copiar/selecionar os CSV, gráficos e `maquina.txt` que serão efetivamente usados no relatório.


## Ajuste metodológico após a primeira campanha real

A primeira execução completa revelou que C2 estava sendo compilado com `-O0` após `affine-super-vectorize`, enquanto C1 e C3 recebiam `-O2 -march=native`. Isso mistura duas variáveis ao mesmo tempo: a decisão de vetorização e o nível de otimização final do LLVM.

A partir do Checkpoint 18, C2 mantém a **vetorização explícita no MLIR**, mas o LLVM IR resultante é compilado com `-O2 -march=native`, igualando o estágio final de C1. Assim:

- C1 = vetorização decidida pelo LLVM + O2;
- C2 = vetorização decidida explicitamente pelo MLIR + O2;
- C0 continua O0 como baseline;
- C3 continua O2 + OpenMP.

Os resultados C2 da campanha anterior devem ser substituídos por uma nova execução de `make bench`. C0/C1/C3 e o experimento C1/C1F permanecem metodologicamente válidos.

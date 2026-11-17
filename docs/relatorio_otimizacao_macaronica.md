# Relatório de otimização — Compilador Macaronica (PjBL 4, Parte C)

**Referência da implementação:** `ricklucek/os-isolados-compiler`, branch `main`, campanha executada em **17/11/2026**.  
**Fonte dos números:** saída do `resultados/benchmarks/summary.csv` fornecida pelo grupo. Os arquivos `raw.csv`, `summary.csv`, `maquina.txt` e gráficos SVG devem acompanhar este relatório em `docs/evidencias_benchmarks/`.

## 1. Objetivo e metodologia

Comparar as configurações **C0** (linha de base, compilação do núcleo em `-O0`), **C1** (LLVM/Clang `-O2 -march=native`), **C2** (vetorização explícita por `affine-super-vectorize` no MLIR, seguida de `-O2 -march=native`) e **C3** (`affine-parallelize`, lowering para OpenMP e compilação `-O2 -march=native -fopenmp`). O experimento suplementar **C1F** habilita flags `fast` em operações de ponto flutuante na IR, para investigar o efeito da reassociação.

Foram utilizados os kernels **elementwise** (soma elemento a elemento), **prodesc** (produto escalar) e, para o experimento de ponto flutuante, **sumfloat** (`somavet`). Os tamanhos são **N = 1.000.000** e **N = 10.000.000** elementos. Há **cinco medições por configuração**, com mediana de tempo de execução (`run_median_ms`) e de compilação (`compile_median_ms`) calculadas separadamente. C3 foi medido com 1, 2, 4 e 8 threads. Ao todo, a campanha gerou **160 amostras brutas e 32 linhas agregadas**. A identificação de CPU, sistema e toolchain deve ser consultada em `maquina.txt`;

As durações representam o tempo médio de chamada do kernel medido pelo harness, não o tempo total de inicialização e execução do programa. Os números são resultados de uma campanha na máquina registrada, não garantias de desempenho geral.

## 2. Resultados principais — tempo de execução

Valores em milissegundos, medianas de 5 medições.

| Kernel | N | C0 | C1 | C2 | C3 (1 thread) | C3 (4 threads) | Ganho C1/C0 |
|---|---:|---:|---:|---:|---:|---:|---:|
| elementwise | 1.000.000 | 2,729907 | 0,573604 | 0,583807 | 0,572446 | 0,586742 | 4,76× |
| elementwise | 10.000.000 | 27,090340 | 8,743512 | 8,506393 | 8,400510 | 8,352972 | 3,10× |
| prodesc | 1.000.000 | 3,446662 | 1,002591 | 1,000592 | 1,013511 | 1,079068 | 3,44× |
| prodesc | 10.000.000 | 33,822481 | 9,905540 | 9,742254 | 9,850771 | 10,004125 | 3,41× |

**Interpretação:**

- As configurações otimizadas foram substancialmente mais rápidas que a linha de base C0 nesta campanha.
- C2 foi aproximadamente **2,71% mais rápida que C1** em `elementwise` com N=10 milhões; com N=1 milhão, C2 foi aproximadamente **1,78% mais lenta** que C1. Logo, não houve vitória sistemática do MLIR sobre o autovetorizador LLVM.
- Aumentar `OMP_NUM_THREADS` em C3 **não produziu ganho consistente**. Para `elementwise` com N=10 milhões, 4 threads (8,352972 ms) quase empataram com 1 thread (8,400510 ms); 8 threads foram mais lentas (9,388883 ms). É necessário conferir a IR/OpenMP gerada e considerar limitações por largura de banda de memória, overhead e características do ambiente antes de atribuir uma causa única.
- No código do projeto, o kernel de `prodesc` é emitido como redução em `scf.for`; a documentação explica que o passe de paralelização de laços `affine` pode não paralelizá-lo. Portanto, o fato de variar o número de threads não prova que a redução tenha efetivamente executado em paralelo.

## 3. Corretude dos resultados

O `elementwise` apresentou o mesmo `checksum_median` em C0, C1, C2 e C3 para cada tamanho: **0,295000017** com N=1 milhão e **0,588999987** com N=10 milhões.

No `prodesc`, o `checksum_median` foi **0**, coerente com o harness: `a[i]` alterna entre `-1` e `+1`, `b[i] = 0,25` e os tamanhos utilizados são pares. Os produtos positivos e negativos se cancelam. Isso é um valor esperado do teste, não necessariamente uma falha.

### Experimento adicional: fast-math

| N | C1 estrito (ms) | C1F fast-math (ms) | Aceleração C1/C1F | Resultado C1 | Resultado C1F | Diferença relativa |
|---|---:|---:|---:|---:|---:|---:|
| 1.000.000 | 0,973553 | 0,077537 | 12,56× | 14,357358 | 14,3927784 | +0,25% |
| 10.000.000 | 9,698932 | 2,005972 | 4,84× | 15,4036827 | 16,6970158 | +8,40% |

O benchmark `sumfloat` soma `1/(i+1)` em `flut` (`float`/32 bits). A sequência de somas e seus arredondamentos importam: a semântica estrita limita reassociações. A variante C1F marca a IR com `fast` e permite transformações antes proibidas. Nesta campanha isso acelerou o cálculo, mas **alterou o resultado numérico**, de maneira especialmente acentuada para N=10 milhões. Assim, não é tecnicamente correto afirmar que fast-math mantém resultados idênticos ou que seu uso é sempre seguro.

## 4. Relação com os conteúdos das aulas

- **Aula 12 — MLIR e lowering:** geração dos kernels em `src/codegen_mlir.c` e aplicação de passes em `scripts/benchmark_variant.sh` (`affine-super-vectorize`, `affine-parallelize`, lowering e tradução para LLVM IR).
- **Aula 13 — Otimizações:** comparação C0–C3, vetorização LLVM versus MLIR, OpenMP, limitações de paralelismo, dependências em reduções e o impacto numérico do fast-math.
- **Metodologia e reprodutibilidade:** `scripts/benchmark.py` registra amostras no `raw.csv`, calcula medianas no `summary.csv`, produz gráficos SVG e salva a descrição do ambiente em `maquina.txt`.

## 5. Conclusão

A campanha completa foi executada e produziu os **160 registros brutos e 32 resumos esperados**. Há ganhos expressivos entre a linha de base C0 e as configurações otimizadas. A vetorização explícita C2 apresentou benefício modesto em parte das condições e não superou C1 em todos os cenários. O número de threads em C3 não apresentou benefício consistente. O caso fast-math demonstra um compromisso real entre velocidade e fidelidade numérica, sendo especialmente útil para a arguição.

**Limitações desta análise:** os dados foram recebidos apenas como tabela textual de medianas; ainda não foram inspecionados os CSV brutos, `maquina.txt`, gráficos nem relatórios `-Rpass` produzidos localmente. A validade final das evidências e a caracterização do ambiente devem ser conferidas diretamente nesses arquivos antes do envio.

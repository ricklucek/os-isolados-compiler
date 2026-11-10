# Decisões do Back-end — Macaronica

Este documento registra as decisões necessárias para PjBL 3 e PjBL 4. Ele complementa `docs/decisoes.md`: as regras do front-end permanecem válidas; aqui são definidos mapeamento para IR, comportamento de execução e portabilidade.

## B01 — Forma de geração da IR

**Decisão:** o compilador continuará em C99 e emitirá LLVM IR textual por `FILE *`/`fprintf`.

**Motivo:** a IR textual é legível, fácil de relacionar com a AST durante a arguição e pode ser validada diretamente por `llvm-as` e `opt -passes=verify`. Não dependemos da API C do LLVM dentro do processo do compilador.

## B02 — Largura fixa dos tipos

A semântica da Macaronica não depende do tamanho de tipos C do alvo:

| Macaronica | LLVM IR |
|---|---|
| `bool` | `i1` |
| `inteira` | `i32` |
| `flut` | `float` (f32) |
| `duplocarpado` | `double` (f64) |
| `palavra` | `ptr` para constante global `[N x i8]` terminada em zero |
| `vazio` | `void` |
| `tipo vet{N}` | `[N x T]` |
| `registro Nome` | `%struct.Nome = type { ... }` |

Nenhum tipo será mapeado para “`int` do C”. Em particular, `inteira` continua sendo 32 bits no Arduino Uno, apesar de o `int` C do AVR ser menor.

## B03 — Promoções numéricas

O back-end reproduz exatamente a promoção já aceita pelo semântico:

```text
inteira -> flut -> duplocarpado
```

Conversões LLVM:

- `i32 -> float`: `sitofp`;
- `i32 -> double`: `sitofp`;
- `float -> double`: `fpext`.

Não há narrowing implícito.

**Serialização de literais `flut`:** o gerador arredonda primeiro o literal para `binary32` e emite a representação hexadecimal exata aceita pelo LLVM 18. Isso evita depender de uma string decimal que pode não representar exatamente o mesmo `float` (casos como `1.65`).

## B04 — Divisão `/` e `//`

- `//` aceita somente `inteira` e gera divisão inteira com sinal (`sdiv i32`);
- `/` é divisão real: se não houver `duplocarpado`, operandos são promovidos para `float` e o resultado é `flut`; se houver `duplocarpado`, o resultado é `double`;
- divisor constante zero é rejeitado antes da emissão da IR utilizável;
- divisor constante não nulo não recebe check redundante;
- divisor dinâmico recebe um branch de guarda e, se zero, chama `zl_erro_execucao(ZL_ERRO_DIV_ZERO, linha)` seguido de `unreachable`.

A regra implementa a política de inserir verificação de execução somente quando o caso não pode ser provado estaticamente.

## B05 — Operadores lógicos

- `E` e `OU` terão **curto-circuito**, preservando efeitos observáveis de chamadas no operando direito;
- `XOR` avalia os dois operandos e gera `xor i1`;
- `NÃO` gera inversão de `i1`.

O curto-circuito será emitido com blocos básicos e controle de fluxo, não com simples `and/or`.

## B06 — `respost` da função principal

A função Macaronica `principal` será emitida como:

```llvm
define i32 @zl_principal() nounwind
```

O valor de `respost` é o código de saída lógico do programa. O runtime de cada plataforma fornece o ponto de entrada real (`main`, `setup`, etc.) e chama `zl_principal`.

## B07 — Overflow inteiro

`inteira` usa aritmética de 32 bits módulo 2^32 para `+`, `-` e `*`. O LLVM será emitido sem as flags `nsw`/`nuw`.

O caso especial `INT32_MIN // -1`, que não é representável em `i32` e é comportamento indefinido para `sdiv`, é protegido antes da instrução. Quando não pode ser descartado estaticamente, a IR verifica a combinação e chama `zl_erro_execucao(ZL_ERRO_DIV_OVERFLOW, linha)`.

Literais `inteira` também precisam caber em `i32`. O codegen aceita `-2147483648` por tratamento específico do unário, aceita positivos até `2147483647` e rejeita magnitudes maiores sem gerar arquivo LLVM utilizável.

## B08 — Limites de vetor

O front-end rejeita índices constantes fora da faixa. Desde o Checkpoint 12, todo acesso vetorial rebaixado também verifica em runtime:

```text
0 <= indice < tamanho
```

antes do `getelementptr`. Em falha, chama `zl_erro_execucao(ZL_ERRO_INDICE, linha)` e termina o bloco com `unreachable`.

Os índices de `getelementptr` são `i32`, conforme a regra de portabilidade adotada. Assim, índices dinâmicos são protegidos e o diagnóstico preserva a linha do programa Macaronica.

## B09 — `^` e `raiz()`

**Decisão fechada no Checkpoint 11:** as duas operações usam helpers de runtime de assinatura fixa.

- `inteira ^ inteira` -> `zl_pow_i32(base, expoente, linha)`;
- `flut ^ flut` -> `zl_pow_flut`;
- `duplocarpado ^ duplocarpado` -> `zl_pow_duplo`;
- `raiz(...)` -> `zl_raiz_flut` ou `zl_raiz_duplo` conforme o tipo inferido.

A IR não chama `pow`/`sqrt` da plataforma diretamente. No POSIX, o runtime implementa os helpers usando libm; no AVR, as mesmas assinaturas poderão usar a implementação disponível para o alvo.

Potência inteira com expoente negativo gera `ZL_ERRO_EXPOENTE_NEGATIVO`. A multiplicação da potência inteira usa aritmética módulo 2^32, coerente com B07. Raiz de valor negativo gera `ZL_ERRO_RAIZ_NEGATIVA`.

## B10 — Entrada e saída

O código gerado nunca chama `printf` ou `scanf` diretamente.

A camada de runtime terá funções tipadas de assinatura fixa, por exemplo:

```c
void zl_imprime_int(int32_t v);
void zl_imprime_bool(int32_t v);
void zl_imprime_flut(float v);
void zl_imprime_duplo(double v);
void zl_imprime_texto(const char *s);
int32_t zl_le_int(void);
```

A implementação POSIX pode usar libc internamente; o programa LLVM não conhece essa libc.

Formato lógico: `VER` e `FAL`. A política exata de casas/significativos dos tipos reais será fixa no runtime POSIX e documentada junto aos testes de execução.

## B11 — IR portável

O arquivo `.ll` produzido pelo compilador:

- não terá `target triple`;
- não terá `target datalayout`;
- não chamará funções variádicas;
- usará larguras explícitas;
- usará `getelementptr` para vetores/registros;
- usará índices `i32`;
- marcará funções geradas como `nounwind`;
- emitirá `@zl_principal`, nunca o ponto de entrada específico da plataforma.

A escolha do alvo ocorrerá posteriormente na etapa de construção.

## B12 — Arduino Uno e `duplocarpado`

**Placa planejada:** Arduino Uno, ATmega328P, AVR, sem sistema operacional.

A semântica global de `duplocarpado` permanece f64. Não vamos rebaixá-lo silenciosamente para f32 no Uno, porque isso mudaria o significado do mesmo programa entre plataformas.

Quando `--alvo=uno` for implementado, programas que exigirem `duplocarpado` serão rejeitados com diagnóstico dependente do alvo enquanto o runtime AVR não fornecer operações f64 completas.

Essa política preserva portabilidade semântica em vez de aceitar um programa com precisão diferente.

## B13 — Textos

Literais `palavra` serão emitidos como constantes globais de bytes terminadas por zero. Como a linguagem atual não possui escapes de string, não há NUL embutido criado por escape.

Os bytes do lexema serão preservados; o runtime é responsável pela apresentação na plataforma.

## B14 — Registros

Cada `registro` vira um tipo LLVM nomeado `%struct.Nome`, preservando a ordem declarada dos campos. O acesso `variavel.campo` é traduzido com `getelementptr` e índice estático do campo.

Não são calculados offsets ou padding manualmente; isso pertence ao data layout escolhido pelo LLVM na construção para o alvo. Registros podem ser locais ou globais.

O layout pode conter campos vetoriais, mas o acesso indexado a esses campos permanece indisponível enquanto a linguagem não tiver uma sintaxe formal para `registro.campo{indice}`.

## B15 — Estratégia SSA

A primeira geração usará a forma “em memória”:

- variável/parâmetro local -> `alloca` no bloco `entry`;
- leitura -> `load`;
- atribuição -> `store`.

Para **variáveis mutáveis**, não construiremos `phi` manualmente. A SSA promovida será obtida com `opt -passes=mem2reg`.

Há uma exceção deliberada: `E` e `OU` com curto-circuito são expressões de valor. O Checkpoint 10 gera um `phi i1` no bloco de merge para combinar a constante do caminho curto com o valor calculado no operando direito. Esse `phi` não substitui o papel do `mem2reg` sobre variáveis; ele representa diretamente a semântica da expressão.

## B16 — AST anotada como contrato do backend

O analisador semântico grava em cada nó de expressão o `LangType inferred_type`.

O futuro `codegen_llvm` deve consumir essa anotação para escolher instruções e conversões. Ele não deve duplicar a inferência de tipos do semântico.

Assim, a fronteira arquitetural é:

```text
AST estrutural -> semântico -> AST anotada -> codegen
```


## Estado do Checkpoint 9

A primeira geração LLVM está ativa para o subconjunto escalar: `principal` inteira sem parâmetros, globais/locais escalares primitivos, atribuições a identificador, retornos, literais, promoções, aritmética básica, comparações, XOR, operadores unários e E/S básica via runtime POSIX.

Ainda são erros controlados de codegen: condicionais/laços, funções de usuário, vetores, registros, `^`, `raiz()` e curto-circuito de `E`/`OU`. Esses casos não geram IR parcial utilizável: a CLI remove o arquivo de saída quando o lowering falha.

A convenção inicial de `saida(expressao)` é imprimir o valor seguido de quebra de linha. `bool` é impresso como `VER`/`FAL`; `flut` e `duplocarpado` usam seis casas decimais no runtime POSIX. Essa formatação poderá ser revisada desde que permaneça documentada e coberta por saídas esperadas.


## Estado do Checkpoint 10

O lowering LLVM agora cobre `cond/casocontrario`, `durante` e `repete`, com blocos básicos e terminadores válidos. `E` e `OU` implementam a decisão B05 com curto-circuito real e `phi i1`.

Variáveis escalares declaradas em blocos internos são pré-alocadas no `entry`, mas só ficam visíveis quando sua declaração é alcançada. O gerador mantém profundidade de escopo e nomes LLVM únicos para sombreamento, permitindo, por exemplo, `%x.addr` e `%x.1.addr`.

O teste de laços também executa `opt -passes=mem2reg` e exige a presença de `phi i32`, servindo como evidência automatizada da transformação para SSA.


## B17 — ABI de funções da Macaronica

Funções de usuário são símbolos globais LLVM com o mesmo nome da linguagem e atributo `nounwind`. A exceção é `principal`, que continua mapeada para `@zl_principal`.

Parâmetros são passados por valor na assinatura LLVM. Dentro da função, cada parâmetro recebe um `alloca` no `entry` e um `store` do `%argN`, seguindo a mesma estratégia das variáveis locais. `mem2reg` pode eliminar esse armazenamento depois.

Chamadas aplicam as promoções já autorizadas pelo semântico antes do `call`. Não há narrowing implícito.

Funções podem chamar funções definidas textualmente depois e podem ser recursivas. A resolução é feita no módulo LLVM pelo nome global.

## Estado do Checkpoint 11

O lowering cobre funções não-`vazio` e `vazio`, parâmetros, chamadas como expressão ou comando, recursão e forward calls. Os testes executáveis incluem quadrado/média, fatorial recursivo, função `vazio`, chamada entre funções em ordem textual inversa e curto-circuito cujo operando direito contém uma chamada com efeito observável.

`^` e `raiz()` também estão fechados pelo contrato de runtime da decisão B09. Vetores e registros permanecem como os principais itens ainda não rebaixados do PjBL 3.


## B18 — Vetores no LLVM

Vetores da Macaronica são arrays LLVM fixos `[N x T]`, não vetores SIMD `<N x T>`. Isso preserva a semântica de armazenamento indexável da linguagem e deixa a vetorização para as fases de otimização/MLIR.

Acesso a elemento usa `getelementptr [N x T], ptr base, i32 0, i32 indice`. Tanto arrays locais quanto globais usam o mesmo padrão.

## Estado do Checkpoint 12

O lowering LLVM cobre vetores e registros locais/globais, leitura e escrita de elementos/campos e bounds check dinâmico com `ZL_ERRO_INDICE`. Os testes exigem IR aceita por `llvm-as/opt`, saída correta no runtime POSIX, presença de `getelementptr` e erro de índice com linha do fonte.

Com isso, as construções estruturais mínimas do PjBL 3 (variáveis, expressões, controle, funções, vetores, registros, texto e E/S) já possuem caminho de geração LLVM. O próximo checkpoint será de fechamento/hardening do PjBL 3, principalmente verificações de execução adicionais, bateria mínima exigida e preparação explícita de portabilidade.


## B19 — Momento das verificações

O backend usa a regra: **provar estaticamente quando possível; verificar em runtime quando necessário**.

Exemplos:

- vetor com índice literal fora da faixa: erro semântico já existente;
- vetor com índice variável: bounds check em runtime;
- divisão por literal zero: erro de codegen antes da execução;
- divisão por literal não zero: sem guarda redundante;
- divisão por variável: guarda de zero em runtime;
- `INT32_MIN // -1`: guarda em runtime somente quando os operandos não permitem eliminar o caso estaticamente.

Essa política reduz overhead e fornece evidência explícita na IR dos checks que realmente dependem de valores de execução.

## Estado do Checkpoint 13

O PjBL 3 fica fechado com uma bateria superior ao mínimo de oito programas executáveis. Há casos específicos para precedência/associatividade, condicionais aninhadas, `durante` e `repete`, recursão, vetor, registro, entrada/saída, tipos mistos, `^`, `raiz()` e diferença entre `/` e `//`.

A CI também executa uma auditoria de portabilidade da IR e um smoke test do mesmo LLVM IR no backend AVR/ATmega328P. Isso não substitui a biblioteca `zl_runtime_avr.c` nem a execução física exigidas pelo PjBL 4 Parte D.


## B20 — Alvo Arduino Uno

O alvo `uno` fixa:
- tripla LLVM: `avr`;
- CPU: `atmega328p`;
- clock: 16 MHz;
- SRAM física: 2048 bytes;
- flash de aplicação considerada: 32256 bytes, preservando a região do bootloader.

### RAM

O compilador reserva 256 bytes de margem para runtime, endereços de retorno e temporários. Portanto a soma estimada de dados globais e do frame local de cada função deve caber em 1792 bytes.

A estimativa usa as larguras semânticas no AVR:
- bool: 1 B;
- inteira: 4 B;
- flut: 4 B;
- palavra/pointer: 2 B;
- vetores: N × tamanho do elemento;
- registros: soma conservadora dos campos.

Como o backend coloca todas as declarações locais de uma função no `entry`, inclusive as de blocos aninhados, a soma estática acompanha a estratégia real do codegen.

Recursão continua permitida. A checagem cobre um frame, não consegue provar profundidade dinâmica; por isso programas recursivos no Uno devem usar profundidade conhecida/pequena.

### duplocarpado

`duplocarpado` mantém o significado de 64 bits da Macaronica. Em vez de silenciosamente rebaixá-lo ao `double` de 32 bits do avr-gcc, `--alvo=uno` o rejeita semanticamente. Isso preserva a semântica entre alvos.

### Pinos

A sintaxe da Macaronica não admite underscore em identificadores. Por isso os nomes de linguagem são `pinomodo`, `pinoescreve`, `pinole` e `espera`. Internamente eles chamam a ABI `zl_pino_modo`, `zl_pino_escreve`, `zl_pino_le` e `zl_espera`.

A numeração é a do Arduino: D0–D13 = 0–13 e A0–A5 = 14–19.


## B21 — Acesso vetorial dinâmico no LLVM AVR 18

Na validação física do Arduino Uno foi observado que GEP dinâmico sobre vetor local, embora válido e correto no PC, produzia endereçamento incorreto quando rebaixado pelo backend AVR do LLVM 18. O comportamento foi reproduzido no `simavr`: índices constantes funcionavam, mas leitura dinâmica não.

Para `--alvo=uno`, o compilador mantém bounds check em `i32` e chama `zl_vetor_endereco(ptr, i32, i32)` após a validação. O helper calcula o endereço SRAM com tipos nativos do avr-gcc. O caminho PC continua usando GEP.

Essa exceção é dependente do alvo e está coberta por testes em emulador e placa real.

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

## B04 — Divisão `/` e `//`

- `//` aceita somente `inteira` e gera divisão inteira com sinal (`sdiv i32`);
- `/` é divisão real: se não houver `duplocarpado`, operandos são promovidos para `float` e o resultado é `flut`; se houver `duplocarpado`, o resultado é `double`;
- divisão por zero será verificada em tempo de execução e chamará `zl_erro_execucao(ZL_ERRO_DIV_ZERO, linha)`.

A política vale também quando o denominador não é conhecido em compilação.

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

O caso especial `INT32_MIN // -1`, que não é representável em `i32` e é problemático para `sdiv`, deverá ser protegido pelo gerador e convertido em erro de execução, em vez de produzir comportamento dependente do back-end.

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

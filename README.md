# os-isolados-compiler

Front-end de compilador da linguagem **Macaronica**, desenvolvido para a disciplina **Linguagens Formais e Compiladores**.

## Integrantes

- Henrique
- João
- Richard

## Objetivo

O projeto começou com o front-end completo da Macaronica em C:

1. análise léxica;
2. análise sintática;
3. análise semântica.

Na Parte 2, o mesmo compilador evolui para geração de LLVM IR, extensão vetorial via MLIR, otimização e execução em plataforma embarcada. O alvo físico planejado pelo grupo é o **Arduino Uno (ATmega328P/AVR)**.

## Estrutura

```text
src/             compilador C (front-end e, progressivamente, back-ends)
tests/validos/   programas Macaronica válidos
tests/invalidos/ programas Macaronica inválidos
tests/robustez/  testes de falhas, recuperação e entradas adversas
tests/niveis/    programas de validação fornecidos na disciplina
tests/dsl/       extensão vetorial do PjBL 4
docs/            gramática, arquitetura e decisões de projeto/backend
examples/        exemplos simples para execução manual
```

## Compilação

Requisitos: GCC com suporte a C99 e `make`.

```bash
make
```

O projeto é compilado normalmente com:

```text
-std=c99 -Wall -Wextra -pedantic
```


## Ferramentas da Parte 2

A versão de referência adotada para o trabalho é **LLVM/MLIR 18.x**, conforme o material da disciplina. O PjBL 3 usa `llvm-as-18`, `opt-18`, `lli-18`, `llc-18` e `clang-18`. O Checkpoint 14 inicia o PjBL 4 pela DSL vetorial; o lowering MLIR entra no checkpoint seguinte.

As decisões de geração, portabilidade e comportamento dependente do alvo ficam em `docs/decisoes_backend.md`.

## Ambiente Docker padronizado

O repositório inclui `Dockerfile` e `docker-compose.yaml` para que o grupo use a mesma toolchain. O código-fonte permanece montado por bind mount em `/usr/src/app`, portanto alterações feitas no host aparecem imediatamente dentro do container, sem reconstruir a imagem.

Construir o ambiente:

```bash
docker compose build
```

Abrir um shell de desenvolvimento:

```bash
docker compose run --rm dev
```

Executar a validação completa no ambiente padronizado:

```bash
make docker-test
```

Executar os sanitizers no ambiente Docker:

```bash
make docker-sanitize
```

Para recompilar e testar automaticamente sempre que um arquivo `.c` ou `.h` mudar:

```bash
make watch
```

A imagem de desenvolvimento usa Ubuntu 24.04 com GCC, LLVM/Clang 18, `make`, `entr` e os runtimes de AddressSanitizer/UndefinedBehaviorSanitizer. O arquivo `.gitattributes` força finais de linha LF nos scripts e fontes relevantes, inclusive quando o repositório é usado em Windows.

## Execução

Pipeline completo — léxico, sintático, AST e semântico. A extensão não é usada para decidir a linguagem; arquivos `.mac` e `.mcn` são aceitos:

```bash
./macaronica caminho/para/programa.mcn
```

Validar um programa criado na linguagem:

```bash
make check FILE=caminho/para/programa.mcn
```

Validar mostrando tokens e AST:

```bash
make inspect FILE=caminho/para/programa.mcn
```

Validar todos os `.mcn`/`.mac` de uma pasta:

```bash
make check-dir DIR=caminho/da/pasta
```

Exibir tokens:

```bash
./macaronica programa.mac --tokens
```

Executar somente o lexer:

```bash
./macaronica programa.mac --lexer-only
```

Executar até o parser, sem análise semântica:

```bash
./macaronica programa.mac --parser-only
```

Exibir a AST:

```bash
./macaronica programa.mac --ast
```

Ajuda da CLI:

```bash
./macaronica --help
```

`--ast` pode ser combinado com `--parser-only`. `--lexer-only` não pode ser combinado com `--ast` ou `--parser-only`.

Exemplo padrão:

```bash
make run
```

Demonstração completa com fluxo de tokens e AST:

```bash
make demo
```

## Geração LLVM IR — PjBL 3

Depois de o programa passar por lexer, parser e semântico, a opção `-o` gera LLVM IR textual:

```bash
./macaronica programa.mcn -o programa.ll
```

Se a entrada for inválida, nenhum arquivo `.ll` novo é produzido. Até o Checkpoint 12, o gerador cobre funções, controle de fluxo, expressões, E/S básica, vetores locais/globais, registros locais/globais e acesso a elementos/campos com `getelementptr`. Índices vetoriais são verificados em tempo de execução antes do acesso.

Validar um arquivo e a IR com LLVM:

```bash
make emit-llvm FILE=tests/execucao/02_variaveis_expressoes.mcn OUT=programa.ll
```

Gerar, ligar com `runtime/zl_runtime_posix.c` e executar:

```bash
make run-llvm FILE=tests/execucao/05_lacos.mcn
```

Para observar a promoção automática para SSA e os `phi` inseridos pelo LLVM:

```bash
make show-mem2reg FILE=tests/execucao/05_lacos.mcn
```

O código gerado define `@zl_principal`; o `main` real fica no runtime da plataforma.

## Funções, chamadas e recursão — Checkpoint 11

Uma função Macaronica como:

```text
outrafuncao inteira quadrado(inteira n) @
[
    respost n * n;
]
```

é emitida na forma:

```llvm
define i32 @quadrado(i32 %arg0) nounwind {
entry:
  %n.addr = alloca i32
  store i32 %arg0, ptr %n.addr
  ...
  ret i32 %resultado
}
```

Parâmetros usam a mesma estratégia em memória das variáveis locais para que `mem2reg` possa promover ambos. Chamadas fazem conversões numéricas explícitas conforme os tipos anotados pela análise semântica.

Referências a funções definidas mais adiante no arquivo são permitidas. O teste `14_forward_entre_funcoes.mcn` cobre uma função que chama outra cuja definição textual aparece depois. Recursão é coberta por `09_recursao_forward.mcn`, com fatorial.

Funções `vazio` geram `define void` e `ret void`. Se uma função `vazio` termina sem `respost;`, o gerador acrescenta o `ret void` final.

## Potência e raiz

O IR não chama `pow`, `sqrt` ou outras funções da libc/libm diretamente. Em vez disso, usa a ABI fixa do runtime:

```text
zl_pow_i32
zl_pow_flut
zl_pow_duplo
zl_raiz_flut
zl_raiz_duplo
```

No runtime POSIX, os casos reais usam `powf/pow/sqrtf/sqrt`; no futuro runtime AVR, essas mesmas assinaturas terão implementação adequada ao alvo.

A potência inteira usa exponenciação por quadratura e multiplicação em `uint32_t`, preservando a política de overflow módulo 2^32. Expoente inteiro negativo gera `ZL_ERRO_EXPOENTE_NEGATIVO`. Raiz de valor negativo gera `ZL_ERRO_RAIZ_NEGATIVA`.

## Vetores e registros — Checkpoint 12

Vetores são armazenados diretamente como arrays LLVM de tamanho fixo:

```llvm
%valores.addr = alloca [4 x i32]
%elemento = getelementptr [4 x i32], ptr %valores.addr, i32 0, i32 %indice
```

Antes de cada acesso, o gerador verifica:

```text
0 <= indice < tamanho
```

Em falha, a IR chama:

```llvm
call void @zl_erro_execucao(i32 1, i32 <linha>) nounwind
unreachable
```

O código `1` corresponde a `ZL_ERRO_INDICE`.

Registros viram tipos LLVM nomeados. Por exemplo:

```text
registro Pessoa @
[
    palavra nome;
    inteira idade;
    flut altura;
]
```

gera:

```llvm
%struct.Pessoa = type { ptr, i32, float }
```

e `pessoa.idade` usa:

```llvm
getelementptr %struct.Pessoa, ptr %pessoa.addr, i32 0, i32 1
```

Nenhum offset é calculado manualmente. Isso mantém a IR independente do `target datalayout` e é importante para o futuro Arduino Uno.

## Fechamento do PjBL 3 — Checkpoint 13

O Checkpoint 13 consolida os casos de borda e a bateria obrigatória do backend.

### Divisão

A política é deliberadamente híbrida:

- divisor constante igual a zero: rejeição durante a geração LLVM, sem criar `.ll`;
- divisor constante comprovadamente não nulo: nenhum check de runtime é emitido;
- divisor calculado/dinâmico: a IR verifica zero antes de `sdiv`/`fdiv`;
- `INT32_MIN // -1`: protegido antes de `sdiv`, com `ZL_ERRO_DIV_OVERFLOW`.

Isso evita comportamento indefinido do LLVM e insere checks somente quando a condição não pode ser provada estaticamente.

### Intervalo de `inteira`

O contrato de backend é `inteira = i32`. Literais positivos acima de `2147483647` são recusados no codegen. A forma `-2147483648` é reconhecida explicitamente como o menor `i32` válido.

Essa checagem ocorre no backend para preservar a compatibilidade dos testes históricos do front-end do PjBL 1, que não definiam largura numérica.

### Auditoria de portabilidade

```bash
make audit-llvm FILE=tests/execucao/17_agregados_globais.mcn
```

verifica automaticamente:

- ausência de `target triple` e `target datalayout`;
- ausência de chamadas diretas a `printf/scanf` e similares;
- ausência de funções variádicas;
- índices de GEP sem `i64`;
- `nounwind` em todas as funções geradas/declaradas.

### Smoke test para Arduino Uno

Ainda não é a execução na placa, mas o mesmo `.ll` já é submetido ao backend AVR:

```bash
make check-avr FILE=tests/execucao/02_variaveis_expressoes.mcn
```

O alvo usa:

```text
-mtriple=avr
-mcpu=atmega328p
```

e exige a geração de um objeto AVR válido. A ligação com `zl_runtime_avr.c`, geração do `.hex` e gravação física entram na Parte D do PjBL 4.

## DSL vetorial — PjBL 4 Parte A

A extensão preserva o estilo da Macaronica e reutiliza os vetores já existentes.

Operações elemento a elemento:

```text
c receba a + b;
c receba a - b;
c receba a * b;
```

Escalar × vetor:

```text
c receba 3 * a;
c receba a * 3;
```

Reduções e produto escalar:

```text
total receba somavet(a);
maior receba maxvet(a);
produto receba prodesc(a, b);
```

Regras principais:

- a DSL aritmética aceita vetores de `inteira`, `flut` e `duplocarpado`;
- operações vetor-vetor exigem o mesmo tamanho;
- `+` e `-` exigem dois vetores;
- `*` admite vetor×vetor e escalar×vetor;
- promoções seguem `inteira -> flut -> duplocarpado`;
- o vetor de destino deve ter o mesmo tamanho e aceitar o tipo promovido;
- `somavet` e `maxvet` devolvem escalar do tipo do elemento;
- `prodesc` devolve escalar com o tipo numérico promovido dos dois vetores.

A especificação formal fica em `docs/dsl.md`.

Neste checkpoint a DSL é analisada até a semântica. Ela **não é expandida para laços LLVM**: `-o` em um programa que contém operação vetorial de primeira classe retorna erro controlado de codegen. O próximo checkpoint emitirá esses núcleos em MLIR e fará o lowering exigido pelo PjBL 4.

## Códigos de saída

| Código | Resultado |
|---:|---|
| 0 | sucesso |
| 1 | uso inválido da CLI |
| 2 | erro léxico |
| 3 | falha de leitura/memória no lexer |
| 4 | erro sintático |
| 5 | falha interna/memória no parser |
| 6 | erro semântico |
| 7 | falha interna/memória no analisador semântico |
| 8 | construção válida ainda não suportada pelo codegen LLVM |
| 9 | falha interna/de escrita no codegen LLVM |

## Testes

Executar toda a suíte:

```bash
make test
```

Ou uma etapa específica:

```bash
make test-lexer
make test-parser
make test-ast
make test-semantic
make test-backend-prep
make test-llvm
make test-dsl
make test-robustness
make test-delivery
make test-levels
```

Para uma verificação opcional com AddressSanitizer e UndefinedBehaviorSanitizer:

```bash
make test-sanitize
```

O alvo com sanitizers depende de suporte do compilador/plataforma e, por isso, não é executado automaticamente por `make test`.

## Convenções finais da Macaronica

Duas lacunas da especificação foram fechadas explicitamente para atender aos casos obrigatórios de entrega.

Literal de `palavra`:

```text
"texto"
```

Entrada e saída:

```text
entrada(destino);
saida(expressao);
```

Essas operações são validadas pelo front-end, mas não executadas, pois o escopo do trabalho termina na análise semântica. Todos os detalhes e justificativas estão em `docs/decisoes.md`.

## Casos oficiais da entrega

Além das suítes por fase, `tests/entrega/` contém cinco programas válidos e cinco inválidos organizados exatamente pelos tipos mínimos exigidos no enunciado. Para validar apenas esse conjunto:

```bash
make test-delivery
```

Há ainda `examples/programa_completo.mac`, que reúne em um único arquivo as principais estruturas da linguagem para demonstração.

## Empacotamento para entrega

```bash
make package
```

O alvo cria `macaronica-entrega.zip` somente com arquivos versionados, evitando binários e `.o` acidentais.

## Limpeza

```bash
make clean
```

## Pipeline atual

```text
fonte Macaronica
      |
      v
    Lexer
      |
    tokens
      |
      v
    Parser
      |
     AST
      |
      v
Analisador semântico
      |
      +-- tabela de símbolos
      +-- escopos
      +-- verificação de tipos
      +-- assinaturas de funções
      +-- retornos
      |
      v
AST anotada com tipos
      |
      v
(back-end LLVM no próximo checkpoint)
```

Cada fase interrompe o pipeline quando encontra uma classe de erro que impede a próxima etapa. Erros recuperáveis dentro da mesma fase são acumulados quando possível. Detalhes estão em `docs/robustez.md`.

## Plano de desenvolvimento

- Checkpoint 0 — estrutura, especificação, decisões e repositório — concluído
- Checkpoint 1 — analisador léxico — concluído
- Checkpoint 2 — gramática e analisador sintático — concluído
- Checkpoint 3 — AST — concluído
- Checkpoint 4 — analisador semântico e tabela de símbolos — concluído
- Checkpoint 5 — tratamento de erros e robustez — concluído
- Checkpoint 6 — testes e documentação final — concluído
- Checkpoint 7 — roteiro de estudo, apresentação e defesa do código — concluído
- Checkpoint 8 — AST anotada e contrato do back-end — concluído
- Checkpoint 9 — geração LLVM: tipos, variáveis e expressões — concluído
- Checkpoint 10 — LLVM: condicionais, laços e curto-circuito — concluído
- Checkpoint 11 — LLVM: funções, chamadas, recursão, potência e raiz — concluído
- Checkpoint 12 — LLVM: vetores, registros, GEP e bounds dinâmicos — concluído
- Checkpoint 13 — fechamento/hardening do PjBL 3 — concluído
- Checkpoint 14 — PjBL 4 Parte A: DSL vetorial, tokens, gramática, AST e semântica — concluído nesta entrega
- Checkpoint 15 — PjBL 4 Parte B: codegen MLIR e pipeline de lowering — próximo


## Material de estudo e apresentação

O Checkpoint 7 adiciona material específico para a defesa do trabalho:

- `docs/roteiro-estudo.md`: explicação completa do compilador, fase por fase;
- `docs/mapa-codigo.md`: caminho entre conceitos, arquivos, estruturas e funções;
- `docs/perguntas-arguicao.md`: checklist de perguntas com respostas esperadas;
- `docs/roteiro-apresentacao.md`: sequência sugerida para a apresentação e demonstração;
- `docs/auditoria-rubrica.md`: mapeamento entre a implementação e os critérios da rubrica.

O objetivo não é decorar respostas, mas conseguir reconstruir o raciocínio do projeto a partir do código.

## Uso de IA generativa

Foi utilizado **ChatGPT, da OpenAI**, como ferramenta de apoio ao desenvolvimento. O apoio incluiu discussão de arquitetura, identificação de ambiguidades da especificação, geração e revisão de trechos de código, elaboração de testes, investigação de falhas, documentação e preparação do ambiente de validação. O grupo revisa as alterações integradas e permanece responsável pelas decisões adotadas e pela compreensão integral do código entregue.

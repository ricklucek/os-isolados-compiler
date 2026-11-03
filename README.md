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

A versão de referência adotada para o trabalho é **LLVM/MLIR 18.x**, conforme o material da disciplina. O Checkpoint 9 já usa `llvm-as-18`, `opt-18`, `lli-18` e `clang-18` para validar e executar a primeira geração LLVM. MLIR será incorporado nos checkpoints do PjBL 4.

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

Se a entrada for inválida, nenhum arquivo `.ll` novo é produzido. No Checkpoint 9, o gerador cobre `principal` inteira sem parâmetros, variáveis escalares, atribuições, retornos, literais, promoções numéricas, comparações, XOR, unários e os builtins básicos `entrada`/`saida` suportados pelo runtime POSIX. Construções ainda não rebaixadas geram diagnóstico de codegen em vez de IR inválida.

Validar um arquivo e a IR com LLVM:

```bash
make emit-llvm FILE=tests/execucao/02_variaveis_expressoes.mcn OUT=programa.ll
```

Gerar, ligar com `runtime/zl_runtime_posix.c` e executar:

```bash
make run-llvm FILE=tests/execucao/03_saida_tipos.mcn
```

O código gerado define `@zl_principal`; o `main` real fica no runtime da plataforma.

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
- Checkpoint 9 — geração LLVM: tipos, variáveis e expressões — concluído nesta entrega
- Checkpoint 10 — LLVM: condicionais e laços — próximo


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

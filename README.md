# os-isolados-compiler

Front-end de compilador da linguagem **Macaronica**, desenvolvido para a disciplina **Linguagens Formais e Compiladores**.

## Integrantes

- Henrique
- João
- Richard

## Objetivo

Implementar em C as três etapas iniciais do front-end de um compilador:

1. análise léxica;
2. análise sintática;
3. análise semântica.

O projeto não gera código de máquina nem código intermediário executável. O resultado é a validação da entrada e a emissão de erros léxicos, sintáticos e semânticos.

## Estrutura

```text
src/             código-fonte C
tests/validos/   programas Macaronica válidos
tests/invalidos/ programas Macaronica inválidos
tests/robustez/  testes de falhas, recuperação e entradas adversas
docs/            gramática, arquitetura e decisões de projeto
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

A imagem de desenvolvimento usa Debian 12 com GCC, `make`, `entr` e os runtimes de AddressSanitizer/UndefinedBehaviorSanitizer. O arquivo `.gitattributes` força finais de linha LF nos scripts e fontes relevantes, inclusive quando o repositório é usado em Windows.

## Execução

Pipeline completo — léxico, sintático, AST e semântico:

```bash
./macaronica caminho/para/programa.mac
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
make test-robustness
make test-delivery
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
programa válido ou diagnósticos
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
- Checkpoint 7 — roteiro de estudo, apresentação e defesa do código — concluído nesta entrega


## Material de estudo e apresentação

O Checkpoint 7 adiciona material específico para a defesa do trabalho:

- `docs/roteiro-estudo.md`: explicação completa do compilador, fase por fase;
- `docs/mapa-codigo.md`: caminho entre conceitos, arquivos, estruturas e funções;
- `docs/perguntas-arguicao.md`: checklist de perguntas com respostas esperadas;
- `docs/roteiro-apresentacao.md`: sequência sugerida para a apresentação e demonstração;
- `docs/auditoria-rubrica.md`: mapeamento entre a implementação e os critérios da rubrica.

O objetivo não é decorar respostas, mas conseguir reconstruir o raciocínio do projeto a partir do código.

## Uso de IA generativa

Foi utilizada IA generativa como ferramenta de apoio na preparação da estrutura inicial do projeto, discussão de arquitetura, identificação de ambiguidades da especificação, revisão de código, elaboração de casos de teste e documentação. O grupo permanece responsável pelas decisões adotadas e pela compreensão integral do código entregue.

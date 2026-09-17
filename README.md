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
docs/            gramática, arquitetura e decisões de projeto
examples/        exemplos simples para execução manual
```

## Compilação

Requisitos: GCC com suporte a C99 e `make`.

```bash
make
```

O projeto é compilado com:

```text
-std=c99 -Wall -Wextra -pedantic
```

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

`--ast` pode ser combinado com `--parser-only`. `--lexer-only` não pode ser combinado com `--ast` ou `--parser-only`.

Exemplo padrão:

```bash
make run
```

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
```

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

## Plano de desenvolvimento

- Checkpoint 0 — estrutura, especificação, decisões e repositório — concluído
- Checkpoint 1 — analisador léxico — concluído
- Checkpoint 2 — gramática e analisador sintático — concluído
- Checkpoint 3 — AST — concluído
- Checkpoint 4 — analisador semântico e tabela de símbolos — concluído nesta entrega
- Checkpoint 5 — tratamento e recuperação de erros
- Checkpoint 6 — testes e documentação final
- Checkpoint 7 — roteiro de estudo e defesa do código

## Uso de IA generativa

Foi utilizada IA generativa como ferramenta de apoio na preparação da estrutura inicial do projeto, discussão de arquitetura, identificação de ambiguidades da especificação, revisão de código, elaboração de casos de teste e documentação. O grupo permanece responsável pelas decisões adotadas e pela compreensão integral do código entregue.

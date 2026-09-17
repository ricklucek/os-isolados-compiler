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

O projeto não gera código de máquina nem código intermediário executável. O resultado será a validação da entrada e a emissão de erros léxicos, sintáticos e semânticos.

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

Pipeline léxico + sintático + construção da AST:

```bash
./macaronica caminho/para/programa.mac
```

Exibir os tokens:

```bash
./macaronica programa.mac --tokens
```

Executar somente a análise léxica:

```bash
./macaronica programa.mac --lexer-only
```

Exibir a AST produzida pelo parser:

```bash
./macaronica programa.mac --ast
```

As opções `--tokens` e `--ast` podem ser combinadas. `--ast` não pode ser usado com `--lexer-only`.

Exemplo padrão:

```bash
make run
```

## Testes

```bash
make test
```

Ou por etapa:

```bash
make test-lexer
make test-parser
make test-ast
```

## Limpeza

```bash
make clean
```

## Plano de desenvolvimento

- Checkpoint 0 — estrutura, especificação, decisões e repositório — concluído
- Checkpoint 1 — analisador léxico — concluído
- Checkpoint 2 — gramática e analisador sintático — concluído
- Checkpoint 3 — AST — concluído nesta entrega
- Checkpoint 4 — analisador semântico e tabela de símbolos
- Checkpoint 5 — tratamento e recuperação de erros
- Checkpoint 6 — testes e documentação final
- Checkpoint 7 — roteiro de estudo e defesa do código

## Uso de IA generativa

Foi utilizada IA generativa como ferramenta de apoio na preparação da estrutura inicial do projeto, discussão de arquitetura, identificação de ambiguidades da especificação, revisão de código, elaboração de casos de teste e documentação. O grupo permanece responsável pelas decisões adotadas e pela compreensão integral do código entregue.

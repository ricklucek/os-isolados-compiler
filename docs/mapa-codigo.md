# Mapa do Código — conceitos, arquivos e funções

Este documento serve como índice técnico para localizar rapidamente onde cada conceito foi implementado.

## 1. Orquestração

### `src/main.c`

Função central: `main()`.

Responsabilidades:

```text
argumentos da CLI
 -> lexer_scan_file()
 -> parser_parse_ast()
 -> semantic_analyze()
 -> liberação da AST e TokenList
 -> código de saída
```

Flags:

- `--tokens`: imprime TokenList;
- `--ast`: imprime AST;
- `--lexer-only`: interrompe após lexer;
- `--parser-only`: interrompe após parser/AST;
- `--help`: mostra contrato da CLI.

## 2. Tokens

### `src/token.h`

Define:

- `TokenType`;
- `Token`;
- `TokenList`.

### `src/token.c`

Funções-chave:

- `token_list_init()`;
- `token_list_append()`;
- `token_list_count_type()`;
- `token_type_name()`;
- `token_list_print()`;
- `token_list_free()`.

Crescimento: vetor dinâmico com `realloc`.

## 3. Lexer

### `src/lexer.c`

Entrada pública:

```c
LexerStatus lexer_scan_file(...);
```

Caminho principal:

```text
lexer_scan_file
  -> scan_source
      -> scan_identifier
      -> scan_number
      -> scan_string
      -> scan_nao
      -> scan_symbol
      -> token_list_append
```

Funções auxiliares importantes:

- `lexer_peek()` e `lexer_peek_next()`: lookahead sem consumir;
- `lexer_advance()`: consome e atualiza linha/coluna;
- `keyword_type()`: diferencia palavra reservada e identificador;
- `report_invalid_char()`: diagnóstico léxico.

## 4. Parser

### `src/parser.c`

Entrada pública:

```c
ParserResult parser_parse_ast(...);
```

Estrutura do programa:

```text
parse_top_level
  -> parse_record
  -> parse_function
  -> parse_principal
  -> parse_variable_declaration
```

Blocos/comandos:

```text
parse_block
  -> parse_statement
      -> parse_variable_declaration
      -> parse_identifier_statement
      -> parse_return_statement
      -> parse_conditional
      -> parse_while
      -> parse_for
```

Expressões:

```text
parse_expression
 -> parse_logical_xor
 -> parse_logical_and
 -> parse_equality
 -> parse_comparison_expression
 -> parse_term
 -> parse_factor
 -> parse_power
 -> parse_unary
 -> parse_primary
```

Construção da árvore:

- `new_node()`;
- `node_from_token()`;
- `add_child()`;
- `make_binary()`;
- `make_unary()`.

Recuperação:

- `synchronize_statement()`;
- `synchronize_top_level()`;
- `recover_until()`.

## 5. AST

### `src/ast.h`

`AstNodeKind` descreve a natureza de cada nó.

`AstNode` contém tipo do nó, token relacionado, lexema, posição e filhos.

### `src/ast.c`

Funções:

- `ast_node_create()`;
- `ast_node_from_token()`;
- `ast_node_add_child()`;
- `ast_node_kind_name()`;
- `ast_print()`;
- `ast_free()`.

## 6. Tabela de símbolos

### `src/symbol_table.h`

Estruturas:

```text
LangType
SymbolKind
Symbol
SymbolScope
SymbolTable
```

`SymbolScope` possui `parent`, formando a cadeia de escopos.

### `src/symbol_table.c`

Operações centrais:

- `symbol_table_init()`;
- `symbol_table_enter_scope()`;
- `symbol_table_leave_scope()`;
- `symbol_table_declare()`;
- `symbol_table_lookup_current()`;
- `symbol_table_lookup()`;
- `symbol_table_free()`.

Tipos:

- `lang_type_from_token()`;
- `lang_type_can_assign()`;
- `lang_type_common_numeric()`;
- `lang_type_is_numeric()`.

## 7. Semântico

### `src/semantic.c`

Entrada pública:

```c
SemanticResult semantic_analyze(...);
```

Programa:

```text
semantic_analyze
 -> analyze_program
    -> primeira passagem: predeclare_callable
    -> segunda passagem:
       -> analyze_declaration
       -> analyze_record
       -> analyze_function
```

Funções/escopos:

- `predeclare_callable()`: registra assinaturas;
- `analyze_function()`: cria escopo, parâmetros e contexto de retorno;
- `analyze_block()`: cria/encerra escopos internos;
- `declare_symbol()`: centraliza declaração e duplicidade.

Expressões:

- `evaluate_expression()`;
- `evaluate_identifier()`;
- `evaluate_vector_access()`;
- `evaluate_call()`;
- `evaluate_binary()`;
- `evaluate_unary()`;
- `evaluate_root()`.

Comandos:

- `analyze_assignment()`;
- `analyze_return()`;
- `analyze_if()`;
- `analyze_while()`;
- `analyze_for()`.

E/S embutida é tratada dentro de `evaluate_call()`.

## 8. Diagnósticos e códigos de saída

### `src/errors.h/.c`

Define:

- `CompilerPhase`;
- `CompilerExitCode`;
- `compiler_print_phase_success()`;
- `compiler_print_phase_failure()`;
- `compiler_print_internal_failure()`.

As mensagens detalhadas permanecem nos módulos que possuem contexto léxico/sintático/semântico.

## 9. Testes

### Por fase

```text
tests/validos/lexer_*
tests/invalidos/lexer_*
tests/validos/parser_*
tests/invalidos/parser_*
tests/validos/semantic_*
tests/invalidos/semantic_*
```

### AST

`tests/validos/ast_01_completo.mac`.

### Robustez

`tests/robustez/run.sh`.

### Entrega

`tests/entrega/run.sh` valida os cinco casos válidos e cinco inválidos mínimos.

## 10. Ambiente

### `Dockerfile` / `docker-compose.yaml`

Padronizam GCC, make e sanitizers.

### `Makefile`

Comandos mais importantes:

```bash
make
make test
make test-sanitize
make test-delivery
make demo
make docker-test
make docker-sanitize
```

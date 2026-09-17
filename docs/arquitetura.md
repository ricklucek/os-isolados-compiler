# Arquitetura do compilador

## Pipeline

```text
arquivo fonte (.mac)
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
 tabela de símbolos
        |
        v
válido ou lista de erros
```

## Responsabilidade de cada módulo

### `lexer.c/.h`
Converte caracteres em tokens e identifica erros léxicos.

### `token.c/.h`
Define os tipos de tokens e seus metadados, incluindo lexema, linha e coluna.

### `parser.c/.h`
Valida a estrutura gramatical do fluxo de tokens e constrói a AST.

### `ast.c/.h`
Representa declarações, comandos e expressões da linguagem.

### `semantic.c/.h`
Percorre a AST aplicando regras semânticas.

### `symbol_table.c/.h`
Mantém símbolos, tipos, categorias e escopos.

### `errors.c/.h`
Centraliza a representação e apresentação dos erros.

### `main.c`
Orquestra o pipeline completo.

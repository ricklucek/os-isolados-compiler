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
 tabela de símbolos + tipos + escopos
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
Valida a estrutura gramatical do fluxo de tokens por descida recursiva e constrói a AST.

### `ast.c/.h`
Representa declarações, comandos e expressões da linguagem e gerencia a memória da árvore.

### `semantic.c/.h`
Percorre a AST e verifica declaração/uso, tipos, vetores, chamadas de função, argumentos, condições e retornos. Mantém o contexto da função atual e continua a análise após erros semânticos recuperáveis.

### `symbol_table.c/.h`
Implementa tabela de símbolos com escopo global, escopos de função e escopos aninhados. Armazena categoria, tipo, localização da declaração, tamanho de vetores e assinatura de funções.

### `errors.c/.h`
Centraliza o contrato de saída do compilador: nomes das fases, códigos de saída e resumos de sucesso/falha. Os diagnósticos detalhados continuam sendo produzidos pela fase que possui o contexto necessário, sempre usando as posições preservadas em tokens e nós da AST.

### `main.c`
Orquestra o pipeline e interrompe as fases posteriores quando uma fase anterior não produz representação válida.

## Dependências entre fases

```text
lexer -> TokenList
parser(TokenList) -> AstNode PROGRAM
semantic(AstNode) -> SemanticResult
```

A análise semântica não depende da `TokenList`: a AST copia os dados necessários, o que mantém as fases desacopladas.


## Preparação para o back-end (Checkpoint 8)

O sistema de tipos semânticos deixou de pertencer à tabela de símbolos e passou a `language_types.c/.h`. Isso evita que AST e futuros back-ends dependam conceitualmente da implementação da tabela de símbolos.

Cada nó de expressão da AST possui `inferred_type`. A anotação é preenchida pela análise semântica somente depois que o parser constrói uma árvore válida. O futuro `codegen_llvm` deverá **consumir** essa anotação, e não repetir inferência de tipos.

O fluxo passa a ser:

```text
fonte -> tokens -> AST estrutural -> semântico -> AST anotada -> codegen LLVM
```

Com `--parser-only --ast`, a árvore exibida é estrutural. Com execução completa e `--ast`, a impressão ocorre depois do semântico e inclui anotações como `{tipo=inteira}`, `{tipo=flut}` e `{tipo=bool}`.

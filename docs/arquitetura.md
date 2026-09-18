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

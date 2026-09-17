# AST — Abstract Syntax Tree

O Checkpoint 3 conecta o analisador sintático a uma **AST (Abstract Syntax Tree)**. A árvore é construída somente a partir de uma sequência de tokens léxica e sintaticamente válida e passa a ser a representação de entrada da futura análise semântica.

## 1. Por que a AST existe

O fluxo do projeto passa a ser:

```text
arquivo fonte
   ↓
Lexer
   ↓
TokenList
   ↓
Parser
   ↓
AST
   ↓
Analisador semântico (Checkpoint 4)
```

A AST elimina tokens puramente sintáticos que já cumpriram seu papel, como `;`, `,`, parênteses de agrupamento e delimitadores de bloco. Ela mantém a estrutura necessária para representar declarações, comandos, escopos e expressões.

## 2. Estrutura genérica de nó

Todos os nós usam a estrutura `AstNode`:

```c
typedef struct AstNode {
    AstNodeKind kind;
    TokenType token_type;
    char *lexeme;
    int line;
    int column;
    struct AstNode **children;
    size_t child_count;
    size_t child_capacity;
} AstNode;
```

- `kind`: papel estrutural do nó na árvore;
- `token_type`: tipo declarado, operador ou token relacionado ao nó;
- `lexeme`: nome/valor relevante quando existe;
- `line` e `column`: origem do nó no código-fonte;
- `children`: filhos da árvore.

Os lexemas usados pela AST são copiados. Por isso a `TokenList` pode ser liberada depois que a análise das fases seguintes terminar, sem deixar a árvore dependente da memória interna do lexer.

## 3. Principais tipos de nó

### Estrutura do programa

- `PROGRAM`
- `RECORD`
- `FUNCTION`
- `PRINCIPAL`
- `PARAMETER`
- `BLOCK`

### Declarações e comandos

- `VAR_DECL`
- `VECTOR_DECL`
- `ASSIGNMENT`
- `RETURN`
- `IF`
- `ELSE_IF`
- `ELSE`
- `WHILE`
- `FOR`

### Expressões

- `CALL`
- `VECTOR_ACCESS`
- `IDENTIFIER`
- `INTEGER_LITERAL`
- `REAL_LITERAL`
- `BOOL_LITERAL`
- `BINARY_EXPR`
- `UNARY_EXPR`
- `ROOT_EXPR`

## 4. Exemplos de transformação

Código:

```text
resultado receba a + b * 2;
```

AST:

```text
ASSIGNMENT [receba]
|-- IDENTIFIER [resultado]
`-- BINARY_EXPR [+]
    |-- IDENTIFIER [a]
    `-- BINARY_EXPR [*]
        |-- IDENTIFIER [b]
        `-- INTEGER_LITERAL [2]
```

A própria forma da árvore registra que `*` tem precedência maior que `+`.

Para:

```text
soma(a, 2)
```

a árvore contém:

```text
CALL [soma]
|-- IDENTIFIER [a]
`-- INTEGER_LITERAL [2]
```

Para:

```text
numeros{i}
```

é construído:

```text
VECTOR_ACCESS [numeros]
`-- IDENTIFIER [i]
```

## 5. Convenções para declarações

Uma declaração simples:

```text
inteira a, b;
```

é representada por um `VAR_DECL` cujo `token_type` é `TOKEN_INTEIRA` e cujos filhos são os identificadores `a` e `b`.

Uma declaração de vetor:

```text
inteira vet{10} numeros;
```

gera `VECTOR_DECL` com tipo `TOKEN_INTEIRA`; o primeiro filho representa o tamanho `10` e os demais filhos representam os nomes declarados.

Funções e `principal` mantêm o tipo de retorno em `token_type`, o nome em `lexeme`, parâmetros como filhos `PARAMETER` e o corpo como um filho `BLOCK`.

## 6. Construção no parser

O parser continua usando descida recursiva, mas as funções de reconhecimento agora também retornam nós. Por exemplo, os níveis de precedência constroem nós `BINARY_EXPR` à medida que consomem operadores.

A potência permanece associativa à direita:

```text
a ^ b ^ c
```

é estruturado como:

```text
a ^ (b ^ c)
```

enquanto soma, subtração, multiplicação e divisão são construídas associativamente à esquerda.

## 7. Erros e árvore parcial

O tratamento de erros do Checkpoint 2 foi preservado. O parser pode continuar sincronizando e reportando mais de uma falha. Entretanto, se qualquer erro sintático ocorrer, a AST parcial é liberada e não é entregue às fases seguintes.

Essa regra impede que a análise semântica tente interpretar uma árvore cuja estrutura sintática não foi validada.

## 8. Gerenciamento de memória

`ast_free()` percorre recursivamente a árvore e libera:

1. todos os filhos;
2. o vetor de filhos;
3. o lexema copiado;
4. o próprio nó.

Falhas de alocação durante a construção são tratadas como `PARSER_MEMORY_ERROR`, sem `segmentation fault`.

## 9. Visualização

A opção:

```bash
./macaronica programa.mac --ast
```

imprime a árvore em formato textual. Ela existe para depuração, testes e demonstração da estrutura produzida pelo parser.

O fluxo padrão continua executando a construção da AST mesmo sem `--ast`; a flag apenas controla sua exibição.

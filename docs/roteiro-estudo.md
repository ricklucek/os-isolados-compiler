# Roteiro de Estudo e Defesa — os-isolados-compiler

Este documento é o guia principal para Henrique, João e Richard entenderem o compilador inteiro. A regra da apresentação é simples: qualquer integrante deve conseguir explicar qualquer módulo, mesmo que outro tenha liderado sua implementação.

## 1. Visão mental do projeto

O compilador implementa apenas o front-end:

```text
arquivo .mac
   |
   v
Lexer
   |
   | TokenList
   v
Parser por descida recursiva
   |
   | AST
   v
Analisador semântico
   |
   | tabela de símbolos + tipos + escopos
   v
programa válido ou diagnósticos
```

Não existe geração de código nem execução do programa Macaronica.

As três perguntas que devem ser respondidas em qualquer fase são:

1. qual representação a fase recebe?
2. o que ela verifica?
3. qual representação ela entrega para a próxima fase?

## 2. Fluxo real em `main.c`

O `main.c` orquestra as fases:

```text
token_list_init
    |
lexer_scan_file
    |
    +-- erro léxico -> encerra com código 2/3
    |
parser_parse_ast
    |
    +-- erro sintático -> AST parcial é descartada -> código 4/5
    |
semantic_analyze
    |
    +-- erro semântico -> código 6/7
    |
sucesso -> código 0
```

As opções `--lexer-only` e `--parser-only` existem para isolar etapas durante testes e demonstrações. `--tokens` imprime a lista de tokens e `--ast` imprime a árvore.

## 3. Análise léxica

### 3.1 Objetivo

Transformar caracteres em tokens sem interpretar estrutura gramatical ou significado semântico.

Exemplo:

```macaronica
inteira numero;
numero receba 25;
```

vira conceitualmente:

```text
INTEIRA("inteira")
IDENTIFIER("numero")
SEMICOLON(";")
IDENTIFIER("numero")
RECEBA("receba")
INTEGER_LITERAL("25")
SEMICOLON(";")
EOF
```

### 3.2 Lexema x token

Lexema é o texto concreto. Token é a classe.

```text
lexema "inteira" -> TOKEN_INTEIRA
lexema "numero"  -> TOKEN_IDENTIFIER
lexema "25"      -> TOKEN_INTEGER_LITERAL
```

### 3.3 Estruturas principais

`Token` guarda:

```c
TokenType type;
char *lexeme;
int line;
int column;
```

`TokenList` é um vetor dinâmico de tokens. `token_list_append()` copia o lexema, e `token_list_free()` libera as cópias e o vetor.

### 3.4 Reconhecimento

`lexer_scan_file()` lê o arquivo inteiro e chama a varredura do conteúdo.

Dentro do lexer:

- `scan_identifier()`: letras/dígitos e depois consulta a tabela de palavras reservadas;
- `scan_number()`: inteiro ou real;
- `scan_string()`: literal `palavra` entre aspas duplas;
- `scan_symbol()`: operadores e delimitadores;
- tratamento específico de `NÃO` em UTF-8.

### 3.5 Maximal munch

Quando dois tokens compartilham prefixo, o lexer escolhe o maior token válido:

```text
/  -> SLASH
// -> INTEGER_DIV

== -> EQUAL_EQUAL
=  -> inválido
```

### 3.6 Posição

`lexer_advance()` atualiza linha e coluna. Cada token guarda a posição de seu primeiro caractere. Isso permite diagnóstico preciso nas fases posteriores.

### 3.7 Erro léxico

O lexer registra `TOKEN_INVALID`, reporta o erro e continua quando é seguro. Assim pode reportar vários caracteres inválidos em uma mesma execução.

## 4. Análise sintática

### 4.1 Objetivo

Receber `TokenList` e verificar se a sequência pertence à gramática da Macaronica.

O parser não pergunta se uma variável existe. Ele pergunta se a forma da sentença está correta.

Exemplo:

```text
IDENTIFIER RECEBA INTEGER_LITERAL SEMICOLON
```

é uma atribuição válida estruturalmente, mesmo que o identificador ainda seja semanticamente desconhecido.

### 4.2 Descida recursiva

O parser foi escrito manualmente por descida recursiva. Cada não-terminal importante da gramática corresponde a uma função.

Exemplos:

```text
declaracao       -> parse_variable_declaration
bloco            -> parse_block
funcao           -> parse_function
principal        -> parse_principal
condicional      -> parse_conditional
durante          -> parse_while
repete           -> parse_for
expressao        -> parse_expression
```

### 4.3 Precedência

A precedência é codificada pela hierarquia de funções:

```text
OU
XOR
E
== !
< >
+ -
* / //
^
unários
primários
```

Por isso:

```text
a + b * 2
```

é interpretado como:

```text
a + (b * 2)
```

### 4.4 Por que não há recursão à esquerda

Uma produção como:

```text
expr -> expr + termo | termo
```

não serve diretamente para descida recursiva porque `expr` chamaria `expr` antes de consumir tokens.

A gramática foi reescrita em níveis de precedência usando laços e chamadas para níveis mais fortes.

### 4.5 Recuperação

Quando ocorre erro sintático, o parser tenta sincronizar em pontos como:

- `;`;
- `]`;
- início reconhecível de outro comando;
- `)` em estruturas com parênteses.

O objetivo é evitar abortar na primeira falha e também evitar loops infinitos.

## 5. AST

### 5.1 Por que existe

A lista de tokens é linear. O semântico precisa enxergar relações estruturais.

```text
a + b * 2
```

vira:

```text
BINARY_EXPR [+]
|-- IDENTIFIER [a]
`-- BINARY_EXPR [*]
    |-- IDENTIFIER [b]
    `-- INTEGER_LITERAL [2]
```

A árvore já contém a precedência.

### 5.2 Estrutura

Cada `AstNode` guarda:

```c
AstNodeKind kind;
TokenType token_type;
char *lexeme;
int line;
int column;
AstNode **children;
size_t child_count;
size_t child_capacity;
```

Os filhos são dinâmicos.

### 5.3 Independência da TokenList

A AST copia o lexema necessário. Depois do parser, o semântico trabalha com a AST e não precisa reinterpretar a lista de tokens.

### 5.4 Memória

`ast_free()` percorre a árvore recursivamente, libera filhos, vetor de filhos, lexema e o próprio nó.

Se o parser encontra erros, a AST parcial não é entregue ao semântico.

## 6. Análise semântica

### 6.1 Objetivo

Responder perguntas que a gramática não consegue responder:

- o nome foi declarado?
- está no escopo correto?
- o tipo da expressão é compatível?
- uma chamada possui quantidade/tipos corretos?
- o retorno corresponde à assinatura?
- o acesso realmente aponta para um vetor?

### 6.2 Tipos

```text
bool
inteira
flut
palavra
duplocarpado
vazio
```

Literais:

```text
INTEGER_LITERAL -> inteira
REAL_LITERAL    -> flut
STRING_LITERAL  -> palavra
VER/FAL         -> bool
```

### 6.3 Promoção numérica

A promoção implícita segue:

```text
inteira -> flut -> duplocarpado
```

O sentido inverso é rejeitado porque pode perder informação.

### 6.4 Tabela de símbolos

Um `Symbol` armazena nome, categoria, tipo, localização, tamanho de vetor e assinatura da função.

Categorias:

```text
variável
vetor
parâmetro
função
principal
registro
```

### 6.5 Escopos

`SymbolTable` mantém `global` e `current`. Cada `SymbolScope` aponta para o pai.

`symbol_table_lookup()` procura no escopo atual e sobe pela cadeia de pais.

Duplicidade é verificada apenas no escopo atual, por isso sombreamento interno é permitido.

### 6.6 Ordem de declaração

Variáveis são inseridas à medida que o bloco é percorrido. Portanto uso antes da declaração é erro.

Funções são diferentes: assinaturas são pré-declaradas antes da análise dos corpos. Isso permite recursão e chamadas a funções declaradas posteriormente.

### 6.7 Funções

Uma chamada valida:

- existência;
- categoria de função;
- quantidade de argumentos;
- compatibilidade de cada argumento;
- tipo de retorno.

### 6.8 Vetores

O semântico verifica:

- tamanho positivo;
- símbolo declarado como vetor;
- uso com índice;
- índice do tipo `inteira`.

### 6.9 Entrada/saída

Por lacuna da especificação, foram definidas operações embutidas:

```text
entrada(destino);
saida(expressao);
```

`entrada` exige lvalue. `saida` exige uma expressão não-`vazio`.

Elas são validadas, mas não executadas.

## 7. Erros e robustez

Os códigos de saída são estáveis:

```text
0 sucesso
1 CLI inválida
2 erro léxico
3 falha interna do lexer
4 erro sintático
5 falha interna do parser
6 erro semântico
7 falha interna do semântico
```

O princípio é: uma fase posterior só executa quando a representação da fase anterior é válida.

A suíte de robustez cobre múltiplos diagnósticos, arquivo vazio/truncado, bytes inválidos, identificador longo, muitos símbolos e blocos profundamente aninhados.

Os sanitizers verificam erros de memória e comportamento indefinido no ambiente Docker padronizado.

## 8. Decisões da especificação que todos devem conhecer

Principais lacunas resolvidas:

- acesso a vetor: `nome{indice}`;
- chamada: `nome(argumentos)`;
- retorno vazio: ausência de `respost` ou `respost;`;
- `registro`, `outrafuncao`, `edai`, `vet` tratados como palavras estruturais;
- `@` e `,` aceitos embora ausentes do alfabeto listado;
- `NÃO` aceito em UTF-8;
- literal de `palavra`: `"texto"`;
- entrada/saída: `entrada(...)` e `saida(...)`;
- `func` permanece reservado, mas sem produção, porque a especificação usa `outrafuncao`;
- comentários não foram inventados porque não há sintaxe definida e `//` já significa divisão inteira.

Todas estão justificadas em `docs/decisoes.md`.

## 9. Percurso de um exemplo completo

Considere:

```macaronica
inteira principal() @
[
    palavra mensagem;
    inteira numero;
    mensagem receba "inicio";
    entrada(numero);
    saida(mensagem);
    respost 0;
]
```

### Lexer

Reconhece tipos, identificadores, `STRING_LITERAL`, delimitadores e palavras reservadas.

### Parser

Reconhece `principal`, bloco, declarações, atribuição, chamadas e retorno.

### AST

Representa:

```text
PRINCIPAL
`-- BLOCK
    |-- VAR_DECL <PALAVRA>
    |-- VAR_DECL <INTEIRA>
    |-- ASSIGNMENT
    |-- CALL [entrada]
    |-- CALL [saida]
    `-- RETURN
```

### Semântico

Declara `mensagem` e `numero`, valida a atribuição de string para `palavra`, verifica que `numero` é um destino válido de `entrada`, valida `saida` e confirma retorno `inteira`.

## 10. Como estudar o código

Não tente memorizar linhas. Para cada arquivo, faça:

1. identifique as estruturas de dados;
2. encontre a função pública;
3. siga as chamadas principais;
4. escolha um programa pequeno e acompanhe sua transformação;
5. depois altere o programa para gerar um erro daquela fase.

A ordem recomendada:

```text
main.c
token.h / token.c
lexer.c
parser.c
ast.h / ast.c
symbol_table.h / symbol_table.c
semantic.c
errors.h / errors.c
```

## 11. Checklist individual

Cada integrante deve conseguir, sem consultar o código:

- desenhar o pipeline;
- explicar lexema/token;
- explicar maximal munch;
- explicar descida recursiva;
- explicar precedência;
- desenhar uma AST simples;
- explicar tabela de símbolos e cadeia de escopos;
- explicar promoção numérica;
- explicar declaração antes do uso;
- explicar pré-declaração de funções;
- explicar como múltiplos erros são reportados;
- explicar quem aloca/libera TokenList e AST;
- justificar pelo menos cinco decisões de `docs/decisoes.md`;
- executar a demo e interpretar tokens/AST/resultado semântico.

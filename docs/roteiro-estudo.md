# Roteiro de Estudo e Defesa

Este documento será ampliado a cada checkpoint. O objetivo é garantir que Henrique, João e Richard consigam explicar qualquer parte do código.

## 1. Visão geral

Todos devem conseguir desenhar e explicar:

```text
Fonte -> Lexer -> Tokens -> Parser -> AST -> Semântico -> Resultado
```

No Checkpoint 1, o trecho funcional é:

```text
Arquivo .mac -> caracteres -> Lexer -> TokenList
```

## 2. Checkpoint 1 — Análise léxica

### Lexema x token

Lexema é a sequência concreta encontrada no fonte. Em `inteira numero;`, os lexemas são `inteira`, `numero` e `;`.

Token é a classificação desses lexemas:

```text
INTEIRA       "inteira"
IDENTIFIER    "numero"
SEMICOLON     ";"
EOF
```

### Palavra reservada x identificador

O lexer primeiro consome a cadeia que obedece à regra `LETRA (LETRA | DIGITO)*`. Depois compara o lexema com a tabela `KEYWORDS` em `lexer.c`.

```text
inteira   -> TOKEN_INTEIRA
inteiras  -> TOKEN_IDENTIFIER
```

### Autômato mental de identificadores

```text
(q0) --letra--> (q1*)
                 ^  |
                 |  |
             letra/digito
                 |  v
                 +--+
```

`q1` é estado de aceitação. O reconhecimento termina quando o próximo caractere não pertence a `LETRA | DIGITO`.

### Literais numéricos

Uma sequência de dígitos gera `INTEGER_LITERAL`. Se depois dos dígitos houver `.` seguido de outro dígito, o lexer continua e gera `REAL_LITERAL`.

```text
25      -> INTEGER_LITERAL
12.5    -> REAL_LITERAL
-10     -> MINUS + INTEGER_LITERAL("10")
```

O sinal é separado porque sua interpretação como operador unário pertence ao parser.

### Maior correspondência (*maximal munch*)

Quando tokens compartilham prefixo, o lexer tenta o lexema válido mais longo:

```text
/   -> SLASH
//  -> INTEGER_DIV
==  -> EQUAL_EQUAL
```

`=` isolado é inválido porque a Macaronica define `receba` para atribuição e `==` para igualdade.

### Linha e coluna

O `Lexer` mantém `line` e `column`. Caracteres comuns incrementam a coluna; `\n` incrementa a linha e redefine a coluna para 1. O token guarda a posição de seu primeiro caractere.

### TokenList e memória

O lexer produz uma sequência de tokens armazenada em vetor dinâmico. Cada `Token` contém:

```c
TokenType type;
char *lexeme;
int line;
int column;
```

`token_list_append()` copia o lexema; `token_list_free()` libera todos os lexemas e o vetor.

### Recuperação de erro léxico

Um caractere inválido gera `TOKEN_INVALID` e mensagem com linha/coluna. O lexer não encerra imediatamente: continua para encontrar outros erros na mesma execução.

## 3. Caminho do código

1. `main.c` inicializa `TokenList`.
2. `lexer_scan_file()` abre e lê o arquivo.
3. `scan_source()` percorre os caracteres.
4. `scan_identifier()`, `scan_number()` e `scan_symbol()` reconhecem lexemas.
5. `token_list_append()` adiciona cada token.
6. `TOKEN_EOF` marca o fim da entrada.
7. `--tokens` permite inspecionar o fluxo produzido.
8. `token_list_free()` libera a memória.

## 4. Perguntas para a arguição

- Qual a diferença entre lexema e token?
- Como uma palavra reservada é diferenciada de um identificador?
- Por que `//` não é confundido com `/`?
- Por que `=` é inválido e `==` é válido?
- Por que `_nome` é rejeitado?
- Como `NÃO` é tratado mesmo usando UTF-8?
- Por que `-10` gera dois tokens?
- Como linha e coluna são calculadas?
- O que acontece depois de um caractere inválido?
- Quem libera a memória dos lexemas?

## 5. Regra do grupo

Cada integrante pode liderar uma parte, mas nenhum módulo é considerado concluído antes de Henrique, João e Richard conseguirem explicar seu fluxo principal.

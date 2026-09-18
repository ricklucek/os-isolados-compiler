# Especificação Léxica — Macaronica

Este documento define o contrato do analisador léxico implementado no Checkpoint 1. Ele parte da especificação fornecida no enunciado e registra separadamente as decisões necessárias para tornar a linguagem analisável.

## 1. Responsabilidade do lexer

O lexer percorre o arquivo fonte da esquerda para a direita e converte a sequência de caracteres em um fluxo de tokens. Cada token armazena:

- tipo (`TokenType`);
- lexema original;
- linha;
- coluna inicial.

O lexer não decide se uma variável foi declarada, se os tipos são compatíveis ou se a sequência de tokens obedece à gramática. Essas responsabilidades pertencem às fases sintática e semântica.

## 2. Identificadores

Regra adotada, respeitando o alfabeto ASCII apresentado no enunciado:

```text
IDENTIFICADOR = LETRA (LETRA | DIGITO)*
LETRA         = a..z | A..Z
DIGITO        = 0..9
```

O caractere `_` não é aceito em identificadores porque não aparece no alfabeto fornecido. Palavras reservadas são reconhecidas depois que a cadeia candidata a identificador é lida. Se o lexema coincide exatamente com uma palavra reservada, o token específico é emitido; caso contrário, é `IDENTIFIER`. A linguagem é tratada como *case-sensitive*.

## 3. Palavras reservadas e tipos

Palavras listadas explicitamente no enunciado: `receba`, `repete`, `durante`, `cond`, `casocontrario`, `func`, `bool`, `inteira`, `flut`, `palavra`, `duplocarpado`, `principal`, `vazio` e `respost`.

As palavras `registro`, `outrafuncao`, `edai` e `vet` aparecem nas estruturas da própria especificação, embora estejam ausentes da lista explícita de reservadas. Elas são tratadas como palavras estruturais para que essas formas possam ser tokenizadas.

## 4. Booleanos e operadores escritos como palavras

| Lexema | Token |
|---|---|
| `VER` | `VER` |
| `FAL` | `FAL` |
| `OU` | `OU` |
| `E` | `E` |
| `NÃO` | `NAO` |
| `XOR` | `XOR` |
| `raiz` | `RAIZ` |

`NÃO` é aceito literalmente em UTF-8. Não foi criado o alias `NAO`, pois ele não aparece na especificação. `raiz()` é tokenizado como `RAIZ`, `LEFT_PAREN`, `RIGHT_PAREN`; a validade da chamada será verificada nas fases posteriores.

## 5. Operadores simbólicos

| Lexema | Token |
|---|---|
| `+` | `PLUS` |
| `-` | `MINUS` |
| `*` | `STAR` |
| `/` | `SLASH` |
| `//` | `INTEGER_DIV` |
| `^` | `CARET` |
| `<` | `LESS` |
| `>` | `GREATER` |
| `==` | `EQUAL_EQUAL` |
| `!` | `BANG` |

O lexer aplica a regra de **maior correspondência** (*maximal munch*) quando existe sobreposição. Um `=` isolado é erro léxico, pois o enunciado define `receba` para atribuição e `==` para igualdade, mas não define `=` sozinho.

## 6. Delimitadores

São reconhecidos `[`, `]`, `{`, `}`, `(`, `)`, `;`, `,` e `@`. `@` e `,` são aceitos porque aparecem nas estruturas e exemplos da Macaronica, apesar de não constarem no alfabeto listado.

## 7. Literais numéricos

A especificação disponibiliza os tipos `inteira`, `flut` e `duplocarpado`, mas não formaliza a sintaxe dos literais reais. Foi adotado:

```text
INTEIRO = DIGITO+
REAL    = DIGITO+ '.' DIGITO+
```

O ponto `.` não é aceito isoladamente. O sinal negativo não faz parte do literal: `-10` é tokenizado como `MINUS` seguido de `INTEGER_LITERAL`; a interpretação como operador unário será responsabilidade do parser.

## 8. Literal do tipo `palavra`

Como a especificação define o tipo `palavra`, mas não define a sintaxe de literal textual, foi adotado:

```text
STRING = '"' { caractere_exceto_quebra_de_linha_e_aspa } '"'
```

Exemplo:

```text
"Macaronica"
```

O token gerado é `STRING_LITERAL`. A aspa de fechamento é obrigatória e o literal não pode atravessar uma quebra de linha. Não foi definida sintaxe de escapes, pois ela não aparece na especificação original.

## 9. Espaços, posição e erros

Espaços, tabulações, `\r` e `\n` separam lexemas e não geram tokens. Cada token registra a linha e a coluna de início. Quando encontra um caractere não definido, o lexer registra `TOKEN_INVALID`, informa linha e coluna, continua a varredura e retorna falha léxica ao final. Isso permite encontrar múltiplos erros em uma única execução.

## 10. Lacunas que permanecem sem sintaxe

Comentários continuam sem implementação porque a especificação da Macaronica não fornece delimitadores e `//` já representa divisão inteira. A decisão está registrada em `docs/decisoes.md`.

Entrada e saída não exigem novos tokens: `entrada` e `saida` permanecem lexicalmente como `IDENTIFIER` e são reconhecidos como operações embutidas na análise semântica.

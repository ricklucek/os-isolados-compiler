# DSL vetorial da Macaronica — PjBL 4 Parte A

## 1. Objetivo

A extensão adiciona operações vetoriais de primeira classe sobre os vetores já existentes, sem alterar a sintaxe de declaração:

```text
inteira vet{4} a;
flut vet{4} b;
duplocarpado vet{4} c;
```

A Parte A define apenas léxico, gramática, AST e semântica. As operações de primeira classe não são expandidas em laços LLVM. A estratégia adotada para a Parte B será **híbrida**: o programa escalar continua usando o backend LLVM do PjBL 3 e os núcleos vetoriais serão emitidos em MLIR.

## 2. Novos tokens

| Lexema | Token | Papel |
|---|---|---|
| `somavet` | `TOKEN_SOMAVET` | soma/redução |
| `maxvet` | `TOKEN_MAXVET` | máximo/redução |
| `prodesc` | `TOKEN_PRODESC` | produto escalar |

`+`, `-`, `*`, `receba`, parênteses e vírgula já pertenciam à linguagem e são reutilizados.

Os três novos lexemas são palavras reservadas.

## 3. Sintaxe

A gramática do parser escalar continua válida. Os vetores passam a ser valores apenas dentro das regras semânticas da DSL.

Produções relevantes, em EBNF:

```text
dsl_reducao
    ::= "somavet" "(" expressao ")"
     |  "maxvet"  "(" expressao ")"
     |  "prodesc" "(" expressao "," expressao ")"

primario
    ::= ...                              (* regras anteriores *)
     |  dsl_reducao

dsl_atribuicao
    ::= IDENTIFIER "receba" expressao ";"
```

As expressões vetoriais elemento a elemento reutilizam a árvore binária normal:

```text
c receba a + b;
c receba a - b;
c receba a * b;
c receba 3 * a;
c receba a * 3;
```

A distinção entre expressão escalar e vetorial é semântica, não uma segunda gramática de precedência. Assim, as mesmas regras de precedência da Macaronica continuam valendo.

## 4. AST

As operações `somavet`, `maxvet` e `prodesc` usam:

```text
AST_VECTOR_REDUCTION
```

com o `token_type` identificando a operação.

Operações elemento a elemento e escalar×vetor continuam usando:

```text
AST_BINARY_EXPR
```

Isso preserva precedência/associatividade e evita duplicar o parser.

Cada `AstNode` possui anotações semânticas:

```text
inferred_type
inferred_is_vector
inferred_vector_size
```

Exemplo conceitual:

```text
BINARY_EXPR [+] {tipo=flut,vetor=4}
|-- IDENTIFIER [a] {tipo=inteira,vetor=4}
`-- IDENTIFIER [b] {tipo=flut,vetor=4}
```

## 5. Domínio de tipos

As operações vetoriais aritméticas aceitam apenas:

```text
inteira
flut
duplocarpado
```

Vetores `bool` e `palavra` continuam válidos na linguagem base, mas não participam desta DSL aritmética.

As promoções seguem a decisão do PjBL 3:

```text
inteira -> flut -> duplocarpado
```

A promoção é aplicada por elemento.

Exemplos:

```text
vet[4] inteira + vet[4] flut
    -> vet[4] flut

3 * vet[4] duplocarpado
    -> vet[4] duplocarpado
```

Não há narrowing implícito.

## 6. Operações elemento a elemento

### Vetor + vetor

```text
c receba a + b;
```

Regras:

1. `a` e `b` devem ser vetores numéricos;
2. os tamanhos devem ser iguais;
3. o resultado é vetor do mesmo tamanho;
4. o tipo de elemento é o tipo numérico comum.

As mesmas regras valem para `-`.

### Vetor * vetor

```text
c receba a * b;
```

É multiplicação elemento a elemento e segue as mesmas regras de tamanho/promoção.

## 7. Escalar × vetor

Somente `*` aceita exatamente um operando vetorial e um escalar numérico:

```text
c receba 3 * a;
c receba a * 3;
```

O tamanho do resultado é o tamanho do vetor. O tipo do elemento é a promoção entre o tipo do escalar e o tipo do elemento.

Não são válidos:

```text
c receba a + 3;
c receba 3 - a;
```

## 8. Atribuição vetorial

Quando o destino sem índice é um vetor:

```text
c receba <expressao-vetorial>;
```

o analisador exige:

1. resultado vetorial;
2. mesmo tamanho do destino;
3. tipo do resultado atribuível ao tipo do elemento de `c`.

Exemplo válido:

```text
flut vet{4} c;
inteira vet{4} a;
flut vet{4} b;

c receba a + b;
```

Exemplo inválido:

```text
inteira vet{4} c;
flut vet{4} a;

c receba a * 2;
```

porque exigiria narrowing `flut -> inteira`.

## 9. Reduções

### `somavet(v)`

Recebe um vetor numérico e devolve um escalar do tipo do elemento.

```text
inteira vet{4} a;
inteira total;
total receba somavet(a);
```

### `maxvet(v)`

Recebe um vetor numérico e devolve um escalar do tipo do elemento.

### `prodesc(a, b)`

Recebe dois vetores numéricos do mesmo tamanho. O resultado é escalar com o tipo numérico comum/promovido.

```text
inteira vet{4} a;
flut vet{4} b;
flut p;

p receba prodesc(a, b);
```

## 10. Erros semânticos obrigatórios

O front-end rejeita, entre outros:

- operação entre vetores de tamanhos diferentes;
- vetor não numérico em operação DSL;
- `+` ou `-` entre vetor e escalar;
- atribuição de expressão vetorial a escalar;
- atribuição de escalar a vetor inteiro;
- resultado vetorial com tamanho diferente do destino;
- narrowing do tipo do elemento;
- `somavet`/`maxvet` com quantidade de argumentos diferente de 1;
- `prodesc` com quantidade diferente de 2 ou tamanhos incompatíveis.

## 11. Fronteira com o backend

No Checkpoint 14, programas DSL válidos chegam até AST anotada e análise semântica.

O backend LLVM do PjBL 3 **não** transforma essas operações em laços escalares. Ao encontrar uma operação vetorial de primeira classe com `-o`, retorna erro controlado de codegen e remove a saída parcial.

No Checkpoint 15, os nós vetoriais serão extraídos como núcleos MLIR usando dialetos padrão e depois rebaixados até LLVM IR, mantendo o restante do programa no backend já existente.

# Gramática da Macaronica — Checkpoint 2

Este documento formaliza a gramática utilizada pelo analisador sintático. A notação é EBNF: `{ X }` significa repetição, `[ X ]` significa elemento opcional e `|` representa alternativas. Terminais literais aparecem entre aspas e nomes de tokens em maiúsculas.

A gramática preserva as construções apresentadas na especificação e as decisões adicionais registradas em `docs/decisoes.md`.

## 1. Programa e declarações globais

```ebnf
programa        = { declaracao_global }, EOF ;

declaracao_global
                = registro
                | funcao
                | principal
                | declaracao_variavel ;

registro        = "registro", IDENTIFIER, "@", "[",
                  { declaracao_variavel }, "]" ;

funcao          = "outrafuncao", tipo_retorno, IDENTIFIER,
                  "(", [ parametros ], ")", "@", bloco ;

principal       = tipo_retorno, "principal",
                  "(", [ parametros ], ")", "@", bloco ;
```

A palavra reservada `func` permanece sem produção porque a especificação a lista no vocabulário, mas apresenta `outrafuncao` como forma sintática de função não-principal. Essa inconsistência está documentada nas decisões de projeto.

## 2. Tipos e parâmetros

```ebnf
tipo_valor      = "bool"
                | "inteira"
                | "flut"
                | "palavra"
                | "duplocarpado" ;

tipo_retorno    = tipo_valor | "vazio" ;

parametros      = parametro, { ",", parametro } ;
parametro       = tipo_valor, IDENTIFIER ;
```

`vazio` é aceito apenas como tipo de retorno, coerente com a descrição do tipo `void` no enunciado.

## 3. Variáveis e vetores

```ebnf
declaracao_variavel
                = tipo_valor, [ declaracao_vetor ],
                  IDENTIFIER, { ",", IDENTIFIER }, ";" ;

declaracao_vetor
                = "vet", "{", INTEGER_LITERAL, "}" ;

lvalue          = IDENTIFIER, [ "{", expressao, "}" ] ;
```

Acesso a vetor segue a decisão `nome{indice}`. O tamanho declarado precisa ser um literal inteiro no nível sintático; verificações adicionais de tamanho pertencem à fase semântica.

## 4. Blocos e comandos

```ebnf
bloco           = "[", { comando }, "]" ;

comando         = declaracao_variavel
                | atribuicao, ";"
                | chamada, ";"
                | retorno
                | condicional
                | enquanto
                | para ;

atribuicao      = lvalue, "receba", expressao ;

retorno         = "respost", [ expressao ], ";" ;

chamada         = IDENTIFIER, "(", [ argumentos ], ")" ;
argumentos      = expressao, { ",", expressao } ;
```

A forma `respost;` é sintaticamente permitida para funções `vazio`. A verificação de compatibilidade com o tipo de retorno será feita pelo analisador semântico.

## 5. Decisão condicional

A especificação fornece explicitamente as formas `VAR COMPARADOR VAR`, `VAR COMPARADOR NUM` e `NUM COMPARADOR NUM`. O parser preserva essa restrição:

```ebnf
condicao        = operando_condicao, comparador, operando_condicao ;

operando_condicao
                = IDENTIFIER, [ "{", expressao, "}" ]
                | INTEGER_LITERAL
                | REAL_LITERAL ;

comparador      = "<" | ">" | "==" | "!" ;

condicional     = "cond", "(", condicao, ")", "edai", bloco,
                  { "casocontrario", "cond", "(", condicao, ")", "edai", bloco },
                  [ "casocontrario", "edai", bloco ] ;
```

Os operadores lógicos definidos no vocabulário são aceitos em expressões gerais, mas não ampliam silenciosamente a produção explícita de `condicao` fornecida no enunciado.

## 6. Repetições

```ebnf
enquanto        = "durante", "(", condicao, ")", "edai", bloco ;

para            = "repete", "(", atribuicao, ",",
                  condicao, ",", atribuicao, ")",
                  "edai", bloco ;
```

A especificação descreve `repete ( var , condição , iteração )`, mas não formaliza `var` e `iteração`. A implementação interpreta inicialização e iteração como atribuições sem `;`, decisão registrada em `docs/decisoes.md`.

## 7. Expressões e precedência

Para evitar recursão à esquerda e preservar precedência, a expressão é dividida em níveis:

```ebnf
expressao       = xor_logico, { "OU", xor_logico } ;

xor_logico      = e_logico, { "XOR", e_logico } ;

e_logico        = igualdade, { "E", igualdade } ;

igualdade       = comparacao, { ( "==" | "!" ), comparacao } ;

comparacao      = termo, { ( "<" | ">" ), termo } ;

termo           = fator, { ( "+" | "-" ), fator } ;

fator           = potencia, { ( "*" | "/" | "//" ), potencia } ;

potencia        = unario, [ "^", potencia ] ;

unario          = ( "+" | "-" | "NÃO" ), unario
                | primario ;

primario        = INTEGER_LITERAL
                | REAL_LITERAL
                | "VER"
                | "FAL"
                | IDENTIFIER
                | IDENTIFIER, "{", expressao, "}"
                | chamada
                | "raiz", "(", expressao, ")"
                | "(", expressao, ")" ;
```

A potência é associativa à direita pela produção recursiva `potencia = unario [ "^" potencia ]`.

## 8. Estratégia do parser

O parser é implementado manualmente por **descida recursiva**. Cada não-terminal relevante corresponde a uma função em `parser.c`. A implementação usa lookahead limitado para distinguir, por exemplo:

- `tipo principal(...)` de uma declaração global de variável;
- `identificador(...)` de uma atribuição iniciada por identificador.

Não é feita análise semântica neste checkpoint. Portanto, o parser não verifica declaração prévia, compatibilidade de tipos, quantidade/tipo de argumentos ou tipo de retorno.

## 9. Recuperação de erro

O parser não encerra necessariamente na primeira falha. Em comandos, procura pontos de sincronização como `;`, `]` ou o início reconhecível do próximo comando. Em estruturas com parênteses, tenta reencontrar `)` e prosseguir para o bloco. Isso permite relatar mais de um erro sintático em uma execução sem entrar em loop infinito.

## 10. Pontos ainda fora da gramática

A especificação fornecida não formaliza de maneira suficiente:

- literal textual para `palavra`;
- comentários;
- comandos de entrada e saída.

Esses pontos continuam registrados como lacunas e não foram inventados silenciosamente neste checkpoint.

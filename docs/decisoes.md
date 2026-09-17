# Decisões de Projeto — Macaronica

Este documento registra decisões tomadas pelo grupo para resolver lacunas e ambiguidades da especificação. Sempre que a especificação não é suficiente, a decisão é explicitada em vez de ser incorporada silenciosamente ao código.

## D01 — Linguagem implementada

A disciplina é Linguagens Formais e Compiladores; portanto, o grupo implementará a linguagem **Macaronica**.

## D02 — Acesso a vetor

**Status: adotada.**

A especificação define `tipo vet{tamanho} nome`, mas não define o acesso. Foi adotado:

```text
nome{indice}
```

Justificativa: reutiliza o delimitador associado a vetores e mantém o acesso distinguível de uma chamada de função.

## D03 — Chamada de função

**Status: adotada.**

A chamada de função segue:

```text
nome(argumento1, argumento2, ...)
```

É a convenção natural indicada pelo próprio enunciado para a lacuna de chamada de função.

## D04 — Retorno em função `vazio`

**Status: adotada sintaticamente; validação semântica pendente.**

Funções de retorno `vazio` podem omitir `respost` ou utilizar:

```text
respost;
```

`respost valor;` continua sintaticamente reconhecível porque a compatibilidade entre retorno e assinatura pertence ao analisador semântico; nessa fase ele deverá ser rejeitado quando a função for `vazio`.

## D05 — Localização de erros

Tokens armazenam linha e coluna. Lexer e parser usam esses dados nos diagnósticos.

## D06 — AST

O parser será conectado a uma AST no Checkpoint 3. O Checkpoint 2 valida a estrutura sintática sem ainda construir a representação intermediária.

## D07 — Palavras estruturais ausentes da lista de reservadas

**Status: adotada.**

`registro`, `outrafuncao`, `edai` e `vet` aparecem nas estruturas da linguagem, embora não estejam todas na lista explícita de palavras reservadas. O lexer as reconhece como palavras estruturais para tornar analisáveis as formas fornecidas pelo próprio enunciado.

## D08 — Símbolos usados nas estruturas, mas ausentes do alfabeto

**Status: parcialmente adotada.**

`@` e `,` aparecem nas estruturas/exemplos e são reconhecidos pelo lexer, mesmo ausentes do alfabeto listado. Aspas/literais textuais continuam pendentes porque a especificação não fornece uma sintaxe para `palavra`.

## D09 — Operador lógico `NÃO` e alfabeto ASCII

**Status: adotada.**

O lexer aceita literalmente `NÃO` em UTF-8, como escrito na tabela da linguagem. Não foi criado o alias `NAO`, pois ele não aparece na especificação.

## D10 — Entrada e saída na Macaronica

**Status: pendente.**

Os casos obrigatórios pedem entrada/saída, mas a seção da Macaronica não fornece comandos correspondentes. Essa sintaxe não será inventada sem registro; deverá ser definida antes dos testes finais ou confirmada com o professor.

## D11 — Literal do tipo `palavra`

**Status: pendente.**

O tipo `palavra` existe, mas a especificação não formaliza literal textual. Por isso, strings ainda não fazem parte do lexer/parser.

## D12 — `func` versus `outrafuncao`

**Status: adotada.**

`func` aparece na lista de palavras reservadas, enquanto a estrutura de função não-principal usa `outrafuncao`. O lexer mantém `func` como token reservado, mas o parser não atribui uma produção a ele. Funções não-principais são declaradas com `outrafuncao`. Se `func` aparecer onde uma declaração global é esperada, o parser informa explicitamente a inconsistência.

## D13 — Inicialização e iteração de `repete`

**Status: adotada.**

A forma original é `repete ( var , condição , iteração ) edai [ ... ]`, mas `var` e `iteração` não são formalizadas. Foi adotado:

```text
repete ( atribuicao , condicao , atribuicao ) edai [ ... ]
```

As atribuições internas não levam `;`, porque a vírgula é o separador estrutural já indicado na especificação.

## D14 — `raiz()`

**Status: adotada.**

O operador `raiz()` é interpretado sintaticamente como uma operação com uma expressão argumento:

```text
raiz(expressao)
```

A verificação de tipo do argumento será semântica.

## D15 — Declarações globais de variáveis

**Status: adotada.**

A especificação apresenta declaração de variável como estrutura da linguagem sem restringi-la explicitamente ao corpo de funções. O parser aceita declarações globais. A tabela de símbolos do próximo checkpoint distinguirá escopo global e escopos de função.

## D16 — Uso de `vazio`

**Status: adotada.**

`vazio` é aceito apenas como tipo de retorno de função/principal, conforme a observação da especificação de que `void` é apenas tipo de retorno. Declarações `vazio x;` e parâmetros `vazio x` são erros sintáticos.

## D17 — Regra de condição

**Status: adotada literalmente.**

A especificação declara explicitamente:

```text
CONDIÇÃO -> VAR COMPARADOR VAR
CONDIÇÃO -> VAR COMPARADOR NUM
CONDIÇÃO -> NUM COMPARADOR NUM
```

O parser não amplia essa produção para uma expressão booleana arbitrária. Operadores `OU`, `E`, `NÃO` e `XOR` continuam disponíveis em expressões gerais, mas uma condição de `cond`, `durante` ou `repete` deve seguir a forma explícita acima.

## Novas ambiguidades encontradas

Toda nova lacuna deverá ser adicionada contendo descrição, decisão, justificativa e impacto na gramática ou semântica.

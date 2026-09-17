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

**Status: adotada e validada semanticamente.**

Funções de retorno `vazio` podem omitir `respost` ou utilizar:

```text
respost;
```

`respost valor;` é reconhecido sintaticamente para permitir um diagnóstico semântico específico, mas é rejeitado pelo analisador semântico quando a função tem retorno `vazio`.

## D05 — Localização de erros

Tokens armazenam linha e coluna. Lexer, parser e nós da AST preservam esses dados para diagnósticos precisos.

## D06 — AST

**Status: implementada.**

O parser constrói uma AST durante a análise sintática. A árvore é entregue somente quando não existem erros sintáticos; árvores parciais são liberadas. Os nós mantêm tipo estrutural (`AstNodeKind`), token relacionado, lexema quando aplicável, linha/coluna e uma lista dinâmica de filhos.

A AST é a representação de entrada do analisador semântico.

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

**Status: adotada e validada semanticamente.**

O operador `raiz()` é interpretado sintaticamente como uma operação com uma expressão argumento:

```text
raiz(expressao)
```

O argumento deve possuir tipo numérico. O resultado é `flut`, ou `duplocarpado` quando o argumento já possui esse tipo.

## D15 — Declarações globais de variáveis

**Status: adotada e implementada.**

A especificação apresenta declaração de variável como estrutura da linguagem sem restringi-la explicitamente ao corpo de funções. O parser aceita declarações globais e a tabela de símbolos distingue escopo global, escopos de função e escopos aninhados.

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

## D18 — Representação de declarações na AST

**Status: adotada.**

Declarações com vários nomes, como `inteira a, b;`, são representadas por um único nó `VAR_DECL` com um filho `IDENTIFIER` para cada nome. Declarações de vetor usam `VECTOR_DECL`; o primeiro filho guarda o tamanho e os demais guardam os identificadores.

Essa forma mantém na árvore a associação de todos os declaradores ao mesmo tipo e evita duplicar informação sintática.

## D19 — AST somente após sucesso sintático

**Status: adotada.**

O parser pode continuar recuperando-se de erros para produzir múltiplos diagnósticos, mas uma AST parcial nunca segue para a análise semântica. Se `error_count > 0`, toda a árvore construída é liberada.

## D20 — Escopos semânticos

**Status: adotada.**

A tabela de símbolos implementa escopo global, escopo de função e escopos aninhados para blocos de controle. Declarações duplicadas são proibidas no mesmo escopo; sombreamento em um escopo interno é permitido.

## D21 — Pré-declaração de funções

**Status: adotada.**

A análise semântica registra as assinaturas de todas as funções e de `principal` antes de analisar os corpos. Isso permite recursão e chamadas a funções declaradas mais adiante. Variáveis globais e locais continuam respeitando a ordem em que são declaradas.

## D22 — Tipo dos literais reais

**Status: adotada.**

Como a especificação possui `flut` e `duplocarpado`, mas fornece uma única forma léxica para número real, `REAL_LITERAL` é tipado como `flut`. Promoção de `flut` para `duplocarpado` é aceita.

## D23 — Promoção numérica

**Status: adotada.**

Conversões implícitas seguem somente o sentido:

```text
inteira -> flut -> duplocarpado
```

Conversões inversas são rejeitadas para evitar perda implícita de informação. Tipos `bool` e `palavra` não são convertidos implicitamente para números.

## D24 — Semântica de `/` e `//`

**Status: adotada.**

`//` aceita apenas `inteira` e retorna `inteira`. `/` aceita tipos numéricos e retorna `flut`, ou `duplocarpado` se algum operando já for `duplocarpado`.

## D25 — Retorno de funções

**Status: adotada.**

Funções não-`vazio` devem possuir ao menos um `respost` e cada retorno deve ser compatível com a assinatura. Funções `vazio` aceitam ausência de retorno ou `respost;`, mas rejeitam `respost valor;`. A análise de fluxo completa para provar retorno em todos os caminhos não é realizada neste checkpoint.

## D26 — Campos de registro

**Status: adotada parcialmente.**

Campos declarados dentro de um `registro` são verificados quanto a tipo, vetor e duplicidade em um escopo próprio do registro. A especificação não fornece sintaxe para instanciar ou acessar campos de registros, portanto nenhuma operação adicional sobre registros foi inventada.

## Novas ambiguidades encontradas

Toda nova lacuna deverá ser adicionada contendo descrição, decisão, justificativa e impacto na gramática ou semântica.

# Análise Semântica — Macaronica

Este documento descreve as regras implementadas no Checkpoint 4. As regras diretamente exigidas pelo enunciado são separadas das convenções necessárias para pontos em que a especificação da Macaronica não define comportamento de tipos com precisão suficiente.

## 1. Pipeline

Após a construção da AST, o fluxo passa a ser:

```text
fonte -> lexer -> tokens -> parser -> AST -> analisador semântico
                                             |
                                             +-> tabela de símbolos
                                             +-> escopos
                                             +-> tipos
                                             +-> assinaturas de função
                                             +-> retornos
```

A análise semântica somente é executada se as análises léxica e sintática terminarem sem erros.

## 2. Tabela de símbolos

Cada símbolo armazena:

- nome;
- categoria (`variavel`, `vetor`, `parametro`, `funcao`, `principal` ou `registro`);
- tipo;
- linha e coluna da declaração;
- tamanho, quando for vetor;
- tipos dos parâmetros, quando for função.

O escopo global contém funções, `principal`, registros e variáveis globais. Cada função possui um escopo próprio. Blocos de `cond`, `casocontrario`, `durante` e `repete` criam escopos aninhados.

Uma declaração duplicada no mesmo escopo é erro. Uma declaração em escopo interno pode ocultar um símbolo de escopo externo.

## 3. Declaração e uso

Variáveis, parâmetros e vetores devem existir antes de serem usados. Declarações locais são inseridas na tabela à medida que o bloco é percorrido, portanto:

```text
x receba 1;
inteira x;
```

é semanticamente inválido.

Funções são tratadas em duas passagens. Na primeira, todas as assinaturas globais são registradas. Na segunda, seus corpos são analisados. Essa decisão permite recursão e chamada de uma função declarada posteriormente no arquivo sem dispensar a exigência de que a função exista no programa.

## 4. Tipos

Tipos da linguagem:

```text
bool
inteira
flut
palavra
duplocarpado
vazio
```

`vazio` é usado somente como retorno.

Literais são tipados assim:

```text
INTEGER_LITERAL -> inteira
REAL_LITERAL    -> flut
STRING_LITERAL  -> palavra
VER / FAL       -> bool
```

Como a sintaxe fornecida não distingue literal `flut` de literal `duplocarpado`, literal real é considerado `flut`; ele pode ser promovido para `duplocarpado`.

## 5. Compatibilidade de atribuição

Atribuições do mesmo tipo são aceitas. Para números, são aceitas promoções sem perda segundo:

```text
inteira -> flut -> duplocarpado
```

O caminho inverso não é implícito. Assim, `flut` não pode ser atribuído diretamente a `inteira`.

Não há conversão implícita entre `bool`, `palavra` e tipos numéricos.

## 6. Operadores

### Aritméticos

`+`, `-`, `*` e `^` exigem operandos numéricos e produzem o tipo numérico de maior capacidade entre os operandos.

`//` exige dois operandos `inteira` e produz `inteira`.

`/` exige operandos numéricos. O resultado é `flut`, exceto quando há `duplocarpado`, caso em que o resultado é `duplocarpado`. Essa distinção preserva o sentido da existência simultânea de `/` e `//`.

`raiz(expressao)` exige valor numérico e produz `flut`, ou `duplocarpado` quando o argumento já é desse tipo.

Os unários `+` e `-` exigem valor numérico.

### Lógicos

`E`, `OU`, `XOR` e `NÃO` exigem operandos `bool` e produzem `bool`.

### Comparadores

`<` e `>` exigem operandos numéricos e produzem `bool`.

`==` e `!` aceitam dois valores do mesmo tipo ou dois tipos numéricos compatíveis e produzem `bool`.

## 7. Vetores

O tamanho declarado deve ser inteiro positivo. O acesso `nome{indice}` exige:

- que `nome` tenha sido declarado como vetor;
- que o índice resulte em `inteira`.

Usar um vetor sem índice como valor ou como destino de atribuição é erro.

## 8. Funções

Uma chamada verifica:

- existência do símbolo;
- categoria de função;
- quantidade de argumentos;
- tipo de cada argumento.

As mesmas regras de promoção numérica de atribuição são utilizadas na passagem de argumentos.

## 9. Retornos

`respost valor;` precisa ser compatível com o tipo de retorno da função.

Função `vazio` pode não possuir `respost` ou usar `respost;`, mas não pode retornar valor.

Função com retorno diferente de `vazio` deve possuir ao menos um `respost` com valor compatível. O Checkpoint 4 verifica presença e tipos de retornos; ele não implementa ainda uma prova completa de que todos os caminhos de controle necessariamente retornam.

## 10. Recuperação e múltiplos erros

O analisador não encerra no primeiro erro semântico. Sempre que a AST permite continuar com segurança, a travessia prossegue e novos diagnósticos são produzidos com linha e coluna.

Erros léxicos ou sintáticos continuam impedindo a execução da fase semântica, pois nesses casos não existe uma AST válida para analisar.


## 11. Entrada e saída embutidas

Para atender ao requisito de teste de entrada/saída diante da ausência de sintaxe correspondente na especificação da Macaronica, o grupo definiu duas chamadas embutidas:

```text
entrada(destino);
saida(expressao);
```

As duas usam a sintaxe normal de chamada de função e, por isso, `entrada` e `saida` continuam sendo identificadores no lexer e nós `CALL` na AST.

Regras:

- `entrada` recebe exatamente um argumento;
- o argumento de `entrada` deve ser um destino atribuível: variável, parâmetro ou acesso a vetor;
- `saida` recebe exatamente uma expressão;
- a expressão de `saida` pode ser `bool`, `inteira`, `flut`, `palavra` ou `duplocarpado`, mas não `vazio`;
- ambas possuem tipo de resultado `vazio`;
- os nomes `entrada` e `saida` são reservados semanticamente e não podem ser declarados pelo programa.

Essas operações são apenas validadas. O compilador não executa o programa de entrada, portanto nenhuma leitura ou escrita real é realizada.

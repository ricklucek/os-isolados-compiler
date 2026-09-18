# Checklist de Perguntas para Arguição

Use este arquivo como treino. Primeiro responda sem olhar. Depois compare com a resposta esperada.

## A. Visão geral

### 1. O que o projeto implementa?
Um front-end de compilador para Macaronica: análise léxica, sintática, construção de AST e análise semântica. Não gera código e não executa o programa.

### 2. Qual a representação entre as fases?
Fonte em caracteres -> `TokenList` -> `AstNode`/AST -> validação semântica apoiada por tabela de símbolos.

### 3. Por que o semântico não trabalha diretamente com tokens?
Porque a AST já registra a estrutura e a precedência. Trabalhar sobre ela desacopla o significado da forma linear dos tokens.

### 4. Por que uma fase posterior não roda quando a anterior falha?
Porque a próxima fase depende de uma representação válida. Erro léxico invalida o token stream; erro sintático impede uma AST confiável.

## B. Lexer

### 5. Qual a diferença entre lexema e token?
Lexema é a sequência concreta do fonte; token é sua classificação.

### 6. Como `inteira` é diferenciada de `inteiras`?
Ambas são lidas como cadeia de letras/dígitos; depois `keyword_type()` procura correspondência exata. `inteira` é reservada, `inteiras` é identificador.

### 7. O que é maximal munch?
Escolher o token válido mais longo quando há prefixos comuns, como `//` antes de `/`.

### 8. Por que `=` isolado é inválido?
Atribuição usa `receba`; igualdade usa `==`. A especificação não define `=`.

### 9. Por que `-10` são dois tokens?
O lexer reconhece `-` como operador. Se é unário ou binário é uma decisão sintática.

### 10. Como `NÃO` é tratado?
Há reconhecimento explícito do lexema UTF-8 `NÃO`, porque o alfabeto declarado não explica o caractere acentuado.

### 11. Como strings são reconhecidas?
`scan_string()` consome a aspa inicial, avança até outra aspa na mesma linha e gera `STRING_LITERAL`. Falta de fechamento gera erro léxico.

### 12. O lexer interpreta o valor de uma string?
Não. Preserva o lexema; a fase semântica apenas classifica o nó como tipo `palavra`.

### 13. Como linha e coluna são calculadas?
`lexer_advance()` incrementa coluna para caracteres comuns e, em `\n`, incrementa linha e volta coluna para 1.

### 14. Como vários erros léxicos são reportados?
O lexer cria `TOKEN_INVALID`, registra o diagnóstico e continua a varredura quando possível.

## C. Parser e gramática

### 15. O que é descida recursiva?
Um parser em que funções correspondem a não-terminais e chamam umas às outras conforme as produções da gramática.

### 16. Por que a gramática de expressões foi dividida em níveis?
Para representar precedência e remover recursão à esquerda incompatível com a implementação direta.

### 17. Como `a + b * 2` ganha a precedência correta?
`parse_term()` chama `parse_factor()`; multiplicação é resolvida em nível mais forte e vira subárvore do lado direito de `+`.

### 18. A potência é associativa para qual lado?
À direita, porque `parse_power()` chama novamente `parse_power()` para o operando direito.

### 19. Como o parser distingue chamada e atribuição começando por identificador?
Usa lookahead: identificador seguido de `(` é chamada; caso contrário segue a forma de lvalue + `receba`.

### 20. O parser verifica se uma variável foi declarada?
Não. Isso é semântica.

### 21. Como o parser recupera erros?
Sincroniza em delimitadores e inícios de comandos usando funções como `synchronize_statement()` e `recover_until()`.

### 22. Por que a AST parcial é descartada se houver erro sintático?
Para impedir que o semântico interprete uma estrutura que não corresponde a um programa sintaticamente válido.

## D. AST

### 23. O que a AST remove em relação aos tokens?
Detalhes sintáticos que não são necessários para representar significado, preservando relações estruturais, operadores, nomes, tipos e posições.

### 24. O que existe em `AstNode`?
Tipo de nó, token relacionado, lexema, linha, coluna, vetor dinâmico de filhos, quantidade e capacidade.

### 25. Como a AST representa `a + b * 2`?
`+` é a raiz; `a` é filho esquerdo; `*` é filho direito com `b` e `2`.

### 26. Quem libera a AST?
`ast_free()`, recursivamente.

### 27. A AST depende da vida útil da TokenList?
Não. Ela copia os lexemas necessários.

## E. Tabela de símbolos e escopos

### 28. O que um `Symbol` armazena?
Nome, categoria, tipo, tamanho de vetor, tipos dos parâmetros, quantidade de parâmetros e posição da declaração.

### 29. Como os escopos são representados?
Cada `SymbolScope` aponta para o pai; `SymbolTable` mantém o global e o atual.

### 30. Como uma busca encontra variável de escopo externo?
`symbol_table_lookup()` procura no escopo atual e sobe pelos ponteiros `parent`.

### 31. Variáveis com mesmo nome podem existir?
Não no mesmo escopo. Em escopo interno, o sombreamento é permitido.

### 32. Como uso antes da declaração é detectado?
Variáveis são declaradas conforme o bloco é percorrido; uma busca anterior à inserção falha.

### 33. Por que funções podem ser chamadas antes da declaração textual?
Há uma primeira passagem que pré-declara todas as assinaturas globais antes de analisar os corpos.

## F. Tipos e semântica

### 34. Quais tipos existem?
`bool`, `inteira`, `flut`, `palavra`, `duplocarpado` e `vazio`.

### 35. Qual o tipo de `10`, `10.5`, `"x"` e `VER`?
`inteira`, `flut`, `palavra` e `bool`.

### 36. Qual promoção numérica é permitida?
`inteira -> flut -> duplocarpado`. O sentido inverso é rejeitado implicitamente.

### 37. Qual a diferença semântica entre `/` e `//`?
`//` exige duas `inteira` e retorna `inteira`. `/` aceita numéricos e retorna `flut` ou `duplocarpado`.

### 38. O que `raiz()` aceita?
Expressão numérica; retorna `flut`, ou `duplocarpado` se o argumento for desse tipo.

### 39. Como chamadas de função são verificadas?
Existência, categoria, quantidade de argumentos e compatibilidade de cada tipo.

### 40. Como vetores são verificados?
Declaração como vetor, tamanho positivo, uso de índice e índice do tipo `inteira`.

### 41. Como retornos são verificados?
`respost valor` deve ser compatível com o retorno da assinatura. Função não-`vazio` precisa de ao menos um retorno; função `vazio` não pode retornar valor.

### 42. O compilador prova que todos os caminhos retornam?
Não. Essa é uma limitação documentada: verifica presença e tipos, não análise de fluxo completa.

### 43. Como `entrada` e `saida` funcionam?
São chamadas embutidas. `entrada` exige lvalue; `saida` uma expressão não-`vazio`. São validadas, não executadas.

## G. Erros, memória e testes

### 44. Quais códigos representam erro léxico, sintático e semântico?
2, 4 e 6, respectivamente.

### 45. Por que existem códigos separados para falha interna?
Para distinguir erro do programa-fonte de falha operacional/memória do compilador.

### 46. Como foi testado que o programa não quebra com entradas adversas?
`tests/robustez/run.sh` cobre arquivos vazios/truncados, bytes inválidos, identificador longo, 256 símbolos, 40 níveis de blocos e múltiplos erros.

### 47. Para que servem ASan e UBSan?
ASan detecta acessos inválidos e problemas de memória; UBSan detecta comportamento indefinido instrumentável.

### 48. Qual comando executa toda a suíte?
`make test`.

### 49. Qual comando executa o conjunto mínimo da entrega?
`make test-delivery`.

### 50. Como reproduzir o ambiente?
`docker compose build` e os alvos `make docker-test` / `make docker-sanitize`.

## H. Decisões e limitações

### 51. Por que `nome{indice}`?
A especificação não define acesso a vetor; a decisão reutiliza a notação de chaves já associada a vetores.

### 52. Por que chamada é `nome(argumentos)`?
É a convenção sugerida pela própria lacuna e é compatível com o estilo da linguagem.

### 53. Por que comentários não foram implementados?
A Macaronica não define delimitadores e `//` já é divisão inteira. Inventar comentário alteraria a linguagem sem base documental.

### 54. Por que `entrada` e `saida` não são palavras reservadas do lexer?
A solução reutiliza a produção de chamada. Elas são identificadores sintaticamente e ganham regra especial apenas no semântico.

### 55. Por que `func` existe no lexer, mas não no parser?
A lista de reservadas contém `func`, porém a estrutura formal usa `outrafuncao`. A inconsistência foi preservada e documentada.

## I. Exercícios orais

Sem olhar o código, expliquem:

1. o percurso completo de `mensagem receba "inicio";`;
2. o percurso completo de `resultado receba soma(1, 2);`;
3. por que `inteira x; flut y; x receba y;` falha;
4. por que um `x` interno pode ocultar um `x` externo;
5. o que acontece com `a + b * 2 ^ c`;
6. onde a memória de um token e de um nó AST é liberada;
7. por que um erro sintático impede análise semântica;
8. como seria adicionado um novo operador à linguagem.

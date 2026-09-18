# Roteiro de Apresentação e Demonstração

Este roteiro foi pensado para uma apresentação curta, aproximadamente 12 a 15 minutos. Ajustem o tempo sem alterar a ordem lógica.

## 1. Abertura — 1 minuto

Apresentar:

- disciplina: Linguagens Formais e Compiladores;
- linguagem: Macaronica;
- integrantes: Henrique, João e Richard;
- escopo: front-end, sem geração de código.

Mensagem central:

```text
Fonte -> Lexer -> Tokens -> Parser -> AST -> Semântico -> Resultado
```

## 2. Especificação e decisões — 2 minutos

Explicar que a especificação contém lacunas e que a própria atividade exige decisões documentadas.

Selecionar quatro exemplos para falar:

1. acesso a vetor `nome{indice}`;
2. chamada `nome(argumentos)`;
3. literal de `palavra` como `"texto"`;
4. `entrada(destino)` e `saida(expressao)`.

Mencionar que comentários não foram inventados porque não há sintaxe definida e `//` já significa divisão inteira.

## 3. Lexer — 2 minutos

Mostrar rapidamente:

```bash
./macaronica examples/programa_completo.mac --tokens --lexer-only
```

Apontar na saída:

- palavra reservada;
- identificador;
- número;
- `STRING_LITERAL`;
- `//`/operadores se desejado;
- linha e coluna;
- EOF.

Explicar maximal munch e recuperação de caracteres inválidos.

## 4. Parser + AST — 3 minutos

Executar:

```bash
./macaronica examples/programa_completo.mac --parser-only --ast
```

Apontar:

- `PROGRAM`;
- `RECORD`;
- `FUNCTION`;
- `PRINCIPAL`;
- `ASSIGNMENT`;
- `CALL`;
- `IF`;
- `FOR`;
- `WHILE`.

Usar uma expressão para explicar precedência:

```text
i receba i + 1
```

Mostrar que a AST guarda a hierarquia.

## 5. Semântico — 3 minutos

Explicar tabela de símbolos e escopos:

```text
global
  -> função
     -> bloco interno
```

Falar de:

- declaração antes do uso;
- duplicidade no mesmo escopo;
- sombreamento;
- assinatura de função;
- tipos;
- vetores;
- retorno.

Mostrar um erro:

```bash
./macaronica tests/entrega/invalidos/04_incompatibilidade_tipo.mac
```

Depois outro, se houver tempo:

```bash
./macaronica tests/entrega/invalidos/05_assinatura_funcao.mac
```

## 6. Robustez e testes — 2 minutos

Mostrar apenas o resumo:

```bash
make test
```

Explicar que a suíte está separada por:

- lexer;
- parser;
- AST;
- semântico;
- robustez;
- casos oficiais da entrega.

Mencionar Docker e sanitizers:

```bash
make docker-test
make docker-sanitize
```

## 7. Fechamento — 1 minuto

Reforçar:

- compilação C99;
- módulos separados;
- AST como fronteira parser/semântico;
- decisões de ambiguidades documentadas;
- múltiplos erros quando possível;
- sem codegen, conforme escopo.

## 8. Divisão sugerida entre os integrantes

Uma divisão possível durante a fala:

- Henrique: contexto, arquitetura, decisões e demonstração final;
- João: lexer, gramática, parser e AST;
- Richard: semântico, tabela de símbolos, testes e robustez.

Isso é apenas divisão de apresentação. Todos precisam dominar as três partes porque a arguição pode direcionar qualquer trecho a qualquer integrante.

## 9. Plano B se a demonstração ao vivo falhar

Não dependa exclusivamente do Docker ao vivo.

Antes da apresentação:

```bash
docker compose run --rm dev sh -lc "make clean && make && make test"
docker compose run --rm dev sh -lc "./macaronica examples/programa_completo.mac --tokens --ast" > demo-output.txt
```

Mantenham o output salvo localmente.

## 10. O que não dizer

Evitar afirmar que:

- o compilador executa programas;
- todos os caminhos de retorno são provados;
- comentários fazem parte da linguagem;
- `entrada` realmente lê teclado;
- `saida` realmente imprime valores do programa.

Essas capacidades não fazem parte da implementação.

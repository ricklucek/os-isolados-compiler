# Auditoria da Implementação contra a Rubrica

Este documento mapeia evidências do repositório para os critérios da rubrica e registra riscos residuais. Ele não substitui a avaliação do professor.

## 1. Analisador léxico — 15%

Evidências:

- `src/lexer.c/.h` separado;
- tokens com linha e coluna;
- palavras reservadas, identificadores, inteiros, reais, strings, booleanos, operadores e delimitadores;
- maximal munch para tokens sobrepostos;
- múltiplos caracteres inválidos em uma execução;
- testes válidos e inválidos específicos.

Ponto documentado: comentários não foram adicionados porque a Macaronica não define sintaxe e `//` já é divisão inteira.

## 2. Analisador sintático — 20%

Evidências:

- gramática EBNF em `docs/gramatica.md`;
- parser manual por descida recursiva;
- precedência e associatividade de expressões;
- estruturas da linguagem cobertas;
- AST construída durante parsing;
- recuperação em múltiplos erros;
- testes sintáticos válidos e inválidos.

## 3. Analisador semântico — 25%

Evidências:

- tabela de símbolos separada;
- escopo global, de função e blocos aninhados;
- declaração antes do uso;
- duplicidade no mesmo escopo;
- sombreamento;
- tipagem de expressões;
- promoções numéricas documentadas;
- vetores;
- assinatura e argumentos de funções;
- retornos;
- entrada/saída embutida.

Limitação: não existe análise de fluxo completa para provar retorno em todos os caminhos.

## 4. Tratamento de erros e robustez — 10%

Evidências:

- linha/coluna em diagnósticos;
- códigos de saída por classe;
- múltiplos erros léxicos, sintáticos e semânticos;
- `tests/robustez/run.sh`;
- arquivo vazio/truncado;
- bytes inválidos;
- identificador longo;
- 256 símbolos;
- 40 níveis de aninhamento;
- ASan/UBSan no ambiente Docker.

## 5. Arquitetura e qualidade — 10%

Módulos:

```text
main
token
lexer
parser
ast
semantic
symbol_table
errors
```

Estruturas dinâmicas possuem rotinas de liberação. A AST desacopla parser e semântico. O Docker padroniza a toolchain.

## 6. Testes e documentação — 10%

Evidências:

- suítes por fase;
- robustez;
- cinco casos válidos oficiais;
- cinco casos inválidos oficiais;
- README;
- gramática;
- decisões;
- especificação léxica;
- semântica;
- arquitetura;
- checklist de entrega;
- material de apresentação/defesa.

## 7. Ambiguidades — 10%

`docs/decisoes.md` registra as decisões e justificativas.

Entre as ambiguidades adicionais identificadas estão:

- palavras estruturais ausentes da lista de reservadas;
- símbolos usados nos exemplos, mas ausentes do alfabeto;
- `NÃO` em UTF-8;
- conflito `func` / `outrafuncao`;
- literal de `palavra`;
- entrada/saída;
- tipo do literal real;
- regras de promoção;
- semântica de `/` e `//`;
- ausência de sintaxe de comentários.

## 8. Critério de compilação

Comando obrigatório:

```bash
make clean
make
```

O projeto usa C99 com:

```text
-Wall -Wextra -pedantic
```

A validação recomendada antes da entrega é:

```bash
make clean
make
make test
make test-sanitize
```

## 9. Checklist antes de enviar

- branch final integrada;
- `make` sem warnings relevantes;
- `make test` verde;
- sanitizers verdes no Docker;
- README atualizado;
- IA declarada;
- `docs/decisoes.md` incluído;
- arquivos temporários/binário não versionados;
- todos os integrantes conseguem explicar o pipeline;
- cada integrante consegue abrir um arquivo aleatório de `src/` e descrever sua responsabilidade.

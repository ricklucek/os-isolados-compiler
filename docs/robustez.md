# Checkpoint 5 — Robustez e Tratamento de Erros

Este checkpoint consolida o comportamento do front-end diante de entradas inválidas e situações operacionais adversas. O objetivo é garantir finalização controlada, diagnósticos úteis e ausência de dependência entre as suítes de testes de cada fase.

## 1. Contrato de códigos de saída

O executável utiliza códigos estáveis para indicar em qual etapa ocorreu a falha:

| Código | Significado |
|---:|---|
| 0 | execução concluída com sucesso |
| 1 | uso inválido da linha de comando |
| 2 | erro léxico na entrada |
| 3 | falha de leitura/memória no lexer |
| 4 | erro sintático |
| 5 | falha interna/memória no parser |
| 6 | erro semântico |
| 7 | falha interna/memória no analisador semântico |

Os códigos são definidos em `src/errors.h`, evitando números mágicos espalhados pelo `main.c`.

## 2. Padronização dos resumos

`errors.c/.h` deixou de ser placeholder e passou a centralizar:

- nome das fases;
- códigos de saída;
- resumo de sucesso;
- resumo de erro;
- resumo de falha interna.

Os diagnósticos específicos continuam sendo produzidos pela fase responsável, porque lexer, parser e analisador semântico possuem contexto próprio para explicar o erro.

## 3. Recuperação de erros

O comportamento esperado é:

```text
erro léxico
  -> registra TOKEN_INVALID
  -> informa linha/coluna
  -> continua a varredura
  -> encerra antes do parser

erro sintático
  -> informa token/localização
  -> tenta sincronizar em delimitadores ou início do próximo comando
  -> acumula diagnósticos
  -> AST parcial é descartada
  -> semântico não executa

erro semântico
  -> registra o diagnóstico
  -> continua percorrendo a AST quando é seguro
  -> acumula problemas independentes
```

Uma fase posterior nunca é executada quando a fase anterior falhou.

## 4. Suíte de robustez

`make test-robustness` executa `tests/robustez/run.sh`. A suíte cobre:

- `--help`, ausência de argumentos, opção desconhecida e flags incompatíveis;
- arquivo inexistente;
- múltiplos caracteres inválidos em uma única análise léxica;
- múltiplos erros sintáticos recuperáveis;
- múltiplos erros semânticos independentes;
- identificador com milhares de caracteres;
- centenas de símbolos no mesmo escopo, exercitando crescimento dinâmico;
- dezenas de blocos aninhados;
- bytes de controle inválidos;
- arquivo vazio;
- arquivo truncado no meio de uma estrutura.

Nos casos em que a validade da entrada não é o ponto do teste, a suíte verifica apenas que o processo termina de maneira controlada, sem encerramento por sinal.

## 5. Sanitizers

Há também um alvo opcional:

```bash
make test-sanitize
```

Ele recompila o projeto com AddressSanitizer e UndefinedBehaviorSanitizer e executa a suíte de robustez. Esse alvo não faz parte de `make test`, pois disponibilidade e comportamento dos sanitizers dependem do compilador/plataforma do ambiente.

## 6. Critério de conclusão

O checkpoint é considerado validado quando:

```bash
make clean
make
make test
```

conclui com sucesso e, em ambiente com suporte aos sanitizers:

```bash
make test-sanitize
```

também conclui sem erros de memória ou comportamento indefinido.

A exigência principal é que nenhuma entrada inválida produza `segmentation fault`, aborto inesperado ou loop infinito; ela deve resultar em diagnóstico e código de saída controlado.

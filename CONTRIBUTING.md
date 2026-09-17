# Fluxo de trabalho do grupo

## Branches

- `main`: checkpoints estáveis
- `develop`: integração do trabalho em andamento
- `feature/...`: implementação de funcionalidades
- `docs/...`: documentação

Sugestões iniciais:

```text
feature/project-structure
feature/lexer
feature/parser
feature/ast
feature/semantic
feature/error-handling
feature/tests
docs/compiler-guide
```

## Pull Requests

Antes de integrar um PR:

1. `make clean && make` deve funcionar;
2. não deve haver warnings críticos;
3. os testes relacionados devem passar;
4. pelo menos outro integrante deve revisar;
5. o autor deve conseguir explicar a alteração para os demais.

## Commits

Exemplos:

```text
chore: initialize compiler project structure
feat: add Macaronica token definitions
feat: implement lexer identifiers and keywords
fix: report invalid token location
docs: document vector access decision
```

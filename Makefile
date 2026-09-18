CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -pedantic
LDFLAGS :=
TARGET := macaronica
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run test test-lexer test-parser test-ast test-semantic test-robustness test-delivery sanitize test-sanitize watch docker-build docker-shell docker-test docker-sanitize

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) examples/exemplo_minimo.mac

test: test-lexer test-parser test-ast test-semantic test-robustness test-delivery

test-lexer: $(TARGET)
	@echo "== Casos lexicos validos =="
	@set -e; for file in tests/validos/lexer_*.mac; do \
		echo "[OK esperado] $$file"; \
		./$(TARGET) "$$file" --lexer-only >/dev/null; \
	done
	@echo "== Casos lexicos invalidos =="
	@set -e; for file in tests/invalidos/lexer_*.mac; do \
		echo "[ERRO esperado] $$file"; \
		if ./$(TARGET) "$$file" --lexer-only >/dev/null 2>&1; then \
			echo "FALHA: $$file deveria ter sido rejeitado pelo lexer."; \
			exit 1; \
		fi; \
	done
	@echo "Todos os testes lexicos passaram."

test-parser: $(TARGET)
	@echo "== Casos sintaticos validos =="
	@set -e; for file in tests/validos/parser_*.mac; do \
		echo "[OK esperado] $$file"; \
		./$(TARGET) "$$file" --parser-only >/dev/null; \
	done
	@echo "== Casos sintaticos invalidos =="
	@set -e; for file in tests/invalidos/parser_*.mac; do \
		echo "[ERRO esperado] $$file"; \
		if ./$(TARGET) "$$file" --parser-only >/dev/null 2>&1; then \
			echo "FALHA: $$file deveria ter sido rejeitado pelo parser."; \
			exit 1; \
		fi; \
	done
	@echo "Todos os testes sintaticos passaram."

test-ast: $(TARGET)
	@echo "== Construcao e estrutura da AST =="
	@./$(TARGET) tests/validos/ast_01_completo.mac --parser-only --ast > .ast-test.out
	@grep -q "PROGRAM" .ast-test.out
	@grep -q "FUNCTION" .ast-test.out
	@grep -q "PRINCIPAL" .ast-test.out
	@grep -q "FOR" .ast-test.out
	@grep -q "IF" .ast-test.out
	@grep -q "CALL" .ast-test.out
	@grep -q "VECTOR_ACCESS" .ast-test.out
	@grep -q "BINARY_EXPR" .ast-test.out
	@rm -f .ast-test.out
	@echo "AST construida com os nos estruturais esperados."

test-semantic: $(TARGET)
	@echo "== Casos semanticamente validos =="
	@set -e; for file in tests/validos/semantic_*.mac; do \
		echo "[OK esperado] $$file"; \
		./$(TARGET) "$$file" >/dev/null; \
	done
	@echo "== Casos semanticamente invalidos =="
	@set -e; for file in tests/invalidos/semantic_*.mac; do \
		echo "[ERRO esperado] $$file"; \
		if ./$(TARGET) "$$file" >/dev/null 2>&1; then \
			echo "FALHA: $$file deveria ter sido rejeitado pelo analisador semantico."; \
			exit 1; \
		fi; \
	done
	@echo "Todos os testes semanticos passaram."

test-robustness: $(TARGET)
	@echo "== Robustez e tratamento de erros =="
	@sh tests/robustez/run.sh ./$(TARGET)

test-delivery: $(TARGET)
	@echo "== Casos oficiais de entrega =="
	@sh tests/entrega/run.sh ./$(TARGET)

sanitize:
	@$(MAKE) clean
	@$(MAKE) CFLAGS="$(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address,undefined" all

test-sanitize: sanitize
	@ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
		UBSAN_OPTIONS=halt_on_error=1 \
		sh tests/robustez/run.sh ./$(TARGET)

watch:
	@command -v entr >/dev/null 2>&1 || { echo "Erro: 'entr' nao esta instalado."; exit 1; }
	@find src -type f \( -name '*.c' -o -name '*.h' \) | \
		entr -r sh -c 'make clean && make && make test'

docker-build:
	docker compose build

docker-shell:
	docker compose run --rm dev

docker-test:
	docker compose run --rm dev sh -lc 'make clean && make && make test'

docker-sanitize:
	docker compose run --rm dev sh -lc 'make test-sanitize'

clean:
	rm -f $(OBJ) $(TARGET) .ast-test.out .robustness-test.out .robustness-test.err

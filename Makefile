CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -pedantic
TARGET := macaronica
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run test test-lexer test-parser

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) examples/exemplo_minimo.mac

test: test-lexer test-parser

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
		./$(TARGET) "$$file" >/dev/null; \
	done
	@echo "== Casos sintaticos invalidos =="
	@set -e; for file in tests/invalidos/parser_*.mac; do \
		echo "[ERRO esperado] $$file"; \
		if ./$(TARGET) "$$file" >/dev/null 2>&1; then \
			echo "FALHA: $$file deveria ter sido rejeitado pelo parser."; \
			exit 1; \
		fi; \
	done
	@echo "Todos os testes sintaticos passaram."

clean:
	rm -f $(OBJ) $(TARGET)

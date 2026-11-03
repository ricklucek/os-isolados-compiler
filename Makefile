CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -pedantic
LDFLAGS :=
TARGET := macaronica
LLVM_AS ?= llvm-as-18
LLVM_OPT ?= opt-18
LLVM_LLI ?= lli-18
CLANG_LLVM ?= clang-18

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run demo check inspect check-dir emit-llvm run-llvm package test test-lexer test-parser test-ast test-semantic test-backend-prep test-llvm test-robustness test-delivery test-levels sanitize test-sanitize watch docker-build docker-shell docker-test docker-sanitize

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) examples/exemplo_minimo.mac

demo: $(TARGET)
	./$(TARGET) examples/programa_completo.mac --tokens --ast

check: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make check FILE=caminho/programa.mcn"; exit 1; }
	@./$(TARGET) "$(FILE)"

inspect: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make inspect FILE=caminho/programa.mcn"; exit 1; }
	@./$(TARGET) "$(FILE)" --tokens --ast

check-dir: $(TARGET)
	@test -n "$(DIR)" || { echo "Uso: make check-dir DIR=caminho/diretorio"; exit 1; }
	@test -n "$(strip $(wildcard $(DIR)/*.mcn $(DIR)/*.mac))" || { echo "Nenhum arquivo .mcn/.mac encontrado em $(DIR)."; exit 1; }
	@set -e; $(foreach file,$(wildcard $(DIR)/*.mcn $(DIR)/*.mac),echo "[VALIDANDO] $(file)"; ./$(TARGET) "$(file)";)

emit-llvm: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make emit-llvm FILE=programa.mcn [OUT=programa.ll]"; exit 1; }
	@./$(TARGET) "$(FILE)" -o "$(if $(OUT),$(OUT),programa.ll)"
	@$(LLVM_AS) "$(if $(OUT),$(OUT),programa.ll)" -o /dev/null
	@$(LLVM_OPT) -passes=verify "$(if $(OUT),$(OUT),programa.ll)" -disable-output
	@echo "LLVM IR validada."

run-llvm: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make run-llvm FILE=programa.mcn"; exit 1; }
	@LLVM_AS="$(LLVM_AS)" LLVM_OPT="$(LLVM_OPT)" CLANG_LLVM="$(CLANG_LLVM)" \
		sh scripts/run_llvm.sh "$(FILE)" ./$(TARGET)

package:
	@rm -f macaronica-entrega.zip
	@git archive --format=zip --output=macaronica-entrega.zip HEAD
	@echo "Pacote criado: macaronica-entrega.zip (somente arquivos versionados)."

test: test-lexer test-parser test-ast test-semantic test-backend-prep test-llvm test-robustness test-delivery test-levels

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

test-backend-prep: $(TARGET)
	@echo "== Preparacao da AST para o backend =="
	@./$(TARGET) tests/validos/semantic_08_tipos_backend.mac --ast > .typed-ast.out
	@grep -Fq 'BINARY_EXPR [+] {tipo=flut}' .typed-ast.out
	@grep -Fq 'BINARY_EXPR [E] {tipo=bool}' .typed-ast.out
	@grep -Fq 'STRING_LITERAL ["ok"] {tipo=palavra}' .typed-ast.out
	@grep -Fq 'CALL [saida] {tipo=vazio}' .typed-ast.out
	@grep -Fq 'BINARY_EXPR [+] {tipo=duplocarpado}' .typed-ast.out
	@rm -f .typed-ast.out
	@echo "AST anotada contem os tipos necessarios ao backend."

test-llvm: $(TARGET)
	@echo "== Geracao e validacao LLVM IR =="
	@LLVM_AS="$(LLVM_AS)" LLVM_OPT="$(LLVM_OPT)" LLVM_LLI="$(LLVM_LLI)" \
		CLANG_LLVM="$(CLANG_LLVM)" sh tests/execucao/run.sh ./$(TARGET)

test-robustness: $(TARGET)
	@echo "== Robustez e tratamento de erros =="
	@sh tests/robustez/run.sh ./$(TARGET)

test-delivery: $(TARGET)
	@echo "== Casos oficiais de entrega =="
	@sh tests/entrega/run.sh ./$(TARGET)

test-levels: $(TARGET)
	@echo "== Programas de nivel fornecidos para validacao =="
	@sh tests/niveis/run.sh ./$(TARGET)

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
	rm -f $(OBJ) $(TARGET) .ast-test.out .typed-ast.out .robustness-test.out .robustness-test.err \
		.llvm-test.ll .llvm-test.bc .llvm-test-prog .llvm-test.out .llvm-invalid.ll \
		.llvm-run.ll .llvm-run macaronica-entrega.zip

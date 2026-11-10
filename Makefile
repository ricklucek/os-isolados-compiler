CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -pedantic
LDFLAGS :=
TARGET := macaronica
LLVM_AS ?= llvm-as-18
LLVM_OPT ?= opt-18
LLVM_LLI ?= lli-18
LLVM_LLC ?= llc-18
LLVM_LINK ?= llvm-link-18
CLANG_LLVM ?= clang-18
MLIR_OPT ?= mlir-opt-18
MLIR_TRANSLATE ?= mlir-translate-18

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run demo check inspect check-dir emit-llvm emit-mlir mlir-pipeline hybrid-build hybrid-run bench-info bench bench-smoke test-bench run-llvm show-mem2reg audit-llvm check-avr package test test-lexer test-parser test-ast test-semantic test-backend-prep test-llvm test-dsl test-mlir test-hybrid test-robustness test-delivery test-levels sanitize test-sanitize watch docker-build docker-shell docker-test docker-sanitize

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

emit-mlir: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make emit-mlir FILE=programa.mcn [OUT=kernels.mlir]"; exit 1; }
	@./$(TARGET) "$(FILE)" --emit-mlir "$(if $(OUT),$(OUT),kernels.mlir)"
	@$(MLIR_OPT) "$(if $(OUT),$(OUT),kernels.mlir)" -o /dev/null
	@echo "MLIR textual validado."

mlir-pipeline: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make mlir-pipeline FILE=programa.mcn [OUTDIR=build/mlir]"; exit 1; }
	@mkdir -p "$(if $(OUTDIR),$(OUTDIR),build/mlir)"
	@./$(TARGET) "$(FILE)" --emit-mlir "$(if $(OUTDIR),$(OUTDIR),build/mlir)/00_input.mlir" >/dev/null
	@MLIR_OPT="$(MLIR_OPT)" MLIR_TRANSLATE="$(MLIR_TRANSLATE)" \
		sh scripts/pipeline.sh "$(if $(OUTDIR),$(OUTDIR),build/mlir)/00_input.mlir" "$(if $(OUTDIR),$(OUTDIR),build/mlir)"

hybrid-build: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make hybrid-build FILE=programa.mcn [OUTDIR=build/hybrid]"; exit 1; }
	@LLVM_AS="$(LLVM_AS)" LLVM_OPT="$(LLVM_OPT)" LLVM_LINK="$(LLVM_LINK)" \
		MLIR_OPT="$(MLIR_OPT)" MLIR_TRANSLATE="$(MLIR_TRANSLATE)" CLANG_LLVM="$(CLANG_LLVM)" \
		sh scripts/hybrid_pipeline.sh "$(FILE)" "$(if $(OUTDIR),$(OUTDIR),build/hybrid)" ./$(TARGET)

hybrid-run: hybrid-build
	@"$(if $(OUTDIR),$(OUTDIR),build/hybrid)/programa"

bench-info:
	@sh scripts/system_info.sh

BENCH_SIZES ?= 1000000,10000000
BENCH_REPS ?= 5
BENCH_THREADS ?= 1,2,4,8
BENCH_OUT ?= resultados/benchmarks

bench: $(TARGET)
	@python3 scripts/benchmark.py \
		--sizes "$(BENCH_SIZES)" \
		--reps "$(BENCH_REPS)" \
		--threads "$(BENCH_THREADS)" \
		--output "$(BENCH_OUT)" \
		--fastmath-experiment

bench-smoke: $(TARGET)
	@rm -rf .bench-smoke
	@python3 scripts/benchmark.py \
		--sizes 1024 \
		--reps 1 \
		--threads 1,2 \
		--configs C0,C1,C2,C3 \
		--kernels elementwise,prodesc \
		--inner 2 \
		--output .bench-smoke
	@test -s .bench-smoke/raw.csv
	@test -s .bench-smoke/summary.csv
	@test -s .bench-smoke/graficos/runtime_elementwise_1024.svg
	@grep -q 'vector<' .bench-smoke/elementwise/1024/C2/variant/01_vector.mlir
	@grep -q 'affine.parallel' .bench-smoke/elementwise/1024/C3/variant/01_parallel.mlir
	@echo "Benchmark smoke C0/C1/C2/C3: OK."

test-bench: bench-smoke

run-llvm: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make run-llvm FILE=programa.mcn"; exit 1; }
	@LLVM_AS="$(LLVM_AS)" LLVM_OPT="$(LLVM_OPT)" CLANG_LLVM="$(CLANG_LLVM)" \
		sh scripts/run_llvm.sh "$(FILE)" ./$(TARGET)

show-mem2reg: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make show-mem2reg FILE=programa.mcn"; exit 1; }
	@./$(TARGET) "$(FILE)" -o .llvm-before.ll >/dev/null
	@$(LLVM_OPT) -S -passes=mem2reg .llvm-before.ll -o .llvm-after.ll
	@echo "Antes:  .llvm-before.ll"
	@echo "Depois: .llvm-after.ll"
	@echo "PHI encontrados:"
	@grep -n ' phi ' .llvm-after.ll || true

audit-llvm: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make audit-llvm FILE=programa.mcn"; exit 1; }
	@./$(TARGET) "$(FILE)" -o .llvm-audit.ll >/dev/null
	@sh scripts/audit_llvm_portability.sh .llvm-audit.ll
	@rm -f .llvm-audit.ll

check-avr: $(TARGET)
	@test -n "$(FILE)" || { echo "Uso: make check-avr FILE=programa.mcn"; exit 1; }
	@./$(TARGET) "$(FILE)" -o .llvm-avr.ll >/dev/null
	@$(LLVM_AS) .llvm-avr.ll -o .llvm-avr.bc
	@$(LLVM_LLC) -mtriple=avr -mcpu=atmega328p -filetype=obj .llvm-avr.bc -o .llvm-avr.o
	@test -s .llvm-avr.o
	@echo "LLVM IR compilada para objeto AVR ATmega328P."
	@rm -f .llvm-avr.ll .llvm-avr.bc .llvm-avr.o

package:
	@rm -f macaronica-entrega.zip
	@git archive --format=zip --output=macaronica-entrega.zip HEAD
	@echo "Pacote criado: macaronica-entrega.zip (somente arquivos versionados)."

test: test-lexer test-parser test-ast test-semantic test-backend-prep test-llvm test-dsl test-mlir test-hybrid test-bench test-robustness test-delivery test-levels

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

test-dsl: $(TARGET)
	@echo "== PjBL 4 Parte A: DSL vetorial =="
	@sh tests/dsl/run.sh ./$(TARGET)

test-mlir: $(TARGET)
	@echo "== PjBL 4 Parte B: MLIR e lowering =="
	@MLIR_OPT="$(MLIR_OPT)" MLIR_TRANSLATE="$(MLIR_TRANSLATE)" LLVM_AS="$(LLVM_AS)" \
		sh tests/mlir/run.sh ./$(TARGET)

test-hybrid: $(TARGET)
	@echo "== PjBL 4: integracao hibrida LLVM + MLIR =="
	@LLVM_AS="$(LLVM_AS)" LLVM_OPT="$(LLVM_OPT)" LLVM_LINK="$(LLVM_LINK)" \
		MLIR_OPT="$(MLIR_OPT)" MLIR_TRANSLATE="$(MLIR_TRANSLATE)" CLANG_LLVM="$(CLANG_LLVM)" \
		sh tests/hybrid/run.sh ./$(TARGET)

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
		.llvm-test.ll .llvm-test.bc .llvm-test-prog .llvm-test.out .llvm-test.err .llvm-invalid.ll \
		.llvm-run.ll .llvm-run .llvm-before.ll .llvm-after.ll .llvm-audit.ll \
		.llvm-avr.ll .llvm-avr.bc .llvm-avr.o .dsl-test.out .dsl-test.ll \
		.mlir-test.mlir .mlir-test.ll macaronica-entrega.zip

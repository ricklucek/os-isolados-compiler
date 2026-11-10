#!/bin/sh
set -u

BIN=${1:-./macaronica}
AST=.dsl-test.out
IR=.dsl-test.ll

cleanup() {
    rm -f "$AST" "$IR"
}
trap cleanup EXIT INT TERM
cleanup

fail() {
    echo "FALHA DSL: $1" >&2
    exit 1
}

for file in tests/dsl/validos/*.mcn; do
    echo "[DSL OK esperado] $file"
    "$BIN" "$file" >/dev/null || fail "$file deveria ser semanticamente valido"
done

for file in tests/dsl/invalidos/*.mcn; do
    echo "[DSL ERRO esperado] $file"
    status=0
    "$BIN" "$file" >/dev/null 2>&1 || status=$?
    [ "$status" -eq 6 ] ||
        fail "$file deveria retornar erro semantico (6), recebeu $status"
done

"$BIN" tests/dsl/validos/01_elementwise.mcn --ast > "$AST" ||
    fail "nao imprimiu AST da operacao elementwise"
grep -Fq 'BINARY_EXPR [+] {tipo=flut,vetor=4}' "$AST" ||
    fail "AST nao anotou resultado vetorial promovido"

"$BIN" tests/dsl/validos/03_reducoes.mcn --tokens --ast > "$AST" ||
    fail "nao imprimiu tokens/AST das reducoes"
grep -Fq 'SOMAVET' "$AST" || fail "TOKEN_SOMAVET ausente"
grep -Fq 'MAXVET' "$AST" || fail "TOKEN_MAXVET ausente"
grep -Fq 'VECTOR_REDUCTION [somavet] {tipo=inteira}' "$AST" ||
    fail "AST de somavet ausente/incorreta"

"$BIN" tests/dsl/validos/04_prodesc_promocao.mcn --tokens --ast > "$AST" ||
    fail "nao imprimiu prodesc"
grep -Fq 'PRODESC' "$AST" || fail "TOKEN_PRODESC ausente"
grep -Fq 'VECTOR_REDUCTION [prodesc] {tipo=flut}' "$AST" ||
    fail "prodesc nao aplicou promocao inteira/flut"

status=0
"$BIN" tests/dsl/validos/01_elementwise.mcn -o "$IR" >/dev/null 2>&1 ||
    status=$?
[ "$status" -eq 8 ] ||
    fail "DSL ainda sem MLIR deveria retornar codegen 8, recebeu $status"
[ ! -e "$IR" ] ||
    fail "codegen DSL deixou LLVM parcial"

echo "DSL vetorial: lexer, parser, AST e semantica OK."

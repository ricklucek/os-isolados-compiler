#!/bin/sh
set -u

BIN=${1:-./macaronica}
LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
LLVM_LLI=${LLVM_LLI:-lli-18}
CLANG_LLVM=${CLANG_LLVM:-clang-18}

IR=.llvm-test.ll
BC=.llvm-test.bc
PROG=.llvm-test-prog
OUT=.llvm-test.out
EXPECTED=.llvm-test.expected
INVALID=.llvm-invalid.ll
UNSUPPORTED=.llvm-unsupported.ll

cleanup() {
    rm -f "$IR" "$BC" "$PROG" "$OUT" "$EXPECTED" "$INVALID" "$UNSUPPORTED"
}
trap cleanup EXIT INT TERM
cleanup

fail() {
    echo "FALHA LLVM: $1" >&2
    exit 1
}

"$BIN" tests/execucao/01_retorno_basico.mcn -o "$IR" >/dev/null ||
    fail "nao gerou retorno basico"

if grep -Eq '^target[[:space:]]+triple' "$IR"; then
    fail "IR contem target triple"
fi
if grep -Eq '^target[[:space:]]+datalayout' "$IR"; then
    fail "IR contem target datalayout"
fi
grep -q 'define i32 @zl_principal() nounwind' "$IR" ||
    fail "principal nao virou zl_principal nounwind"

"$LLVM_AS" "$IR" -o "$BC" || fail "llvm-as rejeitou a IR"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "opt -passes=verify rejeitou a IR"

status=0
"$LLVM_LLI" --entry-function=zl_principal "$IR" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "lli esperava codigo 42 e recebeu $status"

"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou IR + runtime POSIX"

status=0
"./$PROG" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "executavel POSIX esperava codigo 42 e recebeu $status"

"$BIN" tests/execucao/02_variaveis_expressoes.mcn -o "$IR" >/dev/null ||
    fail "nao gerou programa de expressoes"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou programa de expressoes"

status=0
"$LLVM_LLI" --entry-function=zl_principal "$IR" >/dev/null || status=$?
[ "$status" -eq 42 ] ||
    fail "programa de expressoes esperava codigo 42 e recebeu $status"

"$BIN" tests/execucao/03_saida_tipos.mcn -o "$IR" >/dev/null ||
    fail "nao gerou programa de saida"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou programa de saida"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou programa de saida"
"./$PROG" > "$OUT" || fail "programa de saida retornou erro"
tr -d '\r' < tests/execucao/03_saida_tipos.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do runtime POSIX divergiu do esperado"

if "$BIN" tests/invalidos/semantic_01_nao_declarada.mac     -o "$INVALID" >/dev/null 2>&1; then
    fail "fonte semanticamente invalido foi aceito com -o"
fi
[ ! -e "$INVALID" ] ||
    fail "fonte invalido deixou arquivo LLVM"

status=0
"$BIN" tests/validos/semantic_08_tipos_backend.mac     -o "$UNSUPPORTED" >/dev/null 2>&1 || status=$?
[ "$status" -eq 8 ] ||
    fail "construcao ainda nao rebaixada deveria retornar exit 8, recebeu $status"
[ ! -e "$UNSUPPORTED" ] ||
    fail "falha controlada de codegen deixou LLVM parcial"

echo "LLVM IR valida, executavel e portavel nos casos do checkpoint."

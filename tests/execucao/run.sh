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
M2R=.llvm-mem2reg.ll

cleanup() {
    rm -f "$IR" "$BC" "$PROG" "$OUT" "$EXPECTED" "$INVALID" "$UNSUPPORTED" "$M2R"
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

"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
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
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
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

"$BIN" tests/execucao/04_condicional.mcn -o "$IR" >/dev/null ||
    fail "nao gerou condicional"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou condicional"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou condicional"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 2 ] ||
    fail "condicional esperava codigo 2 e recebeu $status"
tr -d '\r' < tests/execucao/04_condicional.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do condicional divergiu"

"$BIN" tests/execucao/05_lacos.mcn -o "$IR" >/dev/null ||
    fail "nao gerou lacos"
"$LLVM_AS" "$IR" -o /dev/null ||
    fail "llvm-as rejeitou lacos"
grep -q 'while.cond' "$IR" || fail "IR nao contem while"
grep -q 'for.cond' "$IR" || fail "IR nao contem for"
"$LLVM_OPT" -S -passes=mem2reg "$IR" -o "$M2R" ||
    fail "mem2reg falhou no programa com lacos"
grep -q 'phi i32' "$M2R" ||
    fail "mem2reg nao produziu phi i32 para variaveis do laco"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou lacos"
status=0
"./$PROG" > "$OUT" || status=$?
[ "$status" -eq 10 ] ||
    fail "lacos esperavam codigo 10 e receberam $status"
tr -d '\r' < tests/execucao/05_lacos.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida dos lacos divergiu"

"$BIN" tests/execucao/06_curto_circuito.mcn -o "$IR" >/dev/null ||
    fail "nao gerou curto-circuito"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou curto-circuito"
grep -q 'phi i1' "$IR" ||
    fail "E/OU nao produziram merge SSA com phi i1"
grep -q 'and.rhs' "$IR" ||
    fail "IR nao contem bloco do lado direito de E"
grep -q 'or.rhs' "$IR" ||
    fail "IR nao contem bloco do lado direito de OU"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou curto-circuito"
"./$PROG" > "$OUT" || fail "programa de curto-circuito retornou erro"
tr -d '\r' < tests/execucao/06_curto_circuito.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do curto-circuito divergiu"

"$BIN" tests/execucao/07_escopo_sombreamento.mcn -o "$IR" >/dev/null ||
    fail "nao gerou sombreamento em bloco interno"
"$LLVM_OPT" -passes=verify "$IR" -disable-output ||
    fail "verify rejeitou sombreamento"
grep -q '%x.addr = alloca i32' "$IR" ||
    fail "armazenamento externo de x ausente"
grep -q '%x.1.addr = alloca i32' "$IR" ||
    fail "armazenamento sombreado de x nao recebeu nome unico"
"$CLANG_LLVM" -std=c99 -Wall -Wextra -pedantic -Wno-override-module     "$IR" runtime/zl_runtime_posix.c -Iruntime -o "$PROG" ||
    fail "clang nao ligou sombreamento"
"./$PROG" > "$OUT" || fail "programa de sombreamento retornou erro"
tr -d '\r' < tests/execucao/07_escopo_sombreamento.esperado > "$EXPECTED"
cmp -s "$OUT" "$EXPECTED" ||
    fail "saida do sombreamento divergiu"

status=0
"$BIN" tests/execucao/99_ainda_nao_suportado.mcn     -o "$UNSUPPORTED" >/dev/null 2>&1 || status=$?
[ "$status" -eq 8 ] ||
    fail "vetor ainda nao rebaixado deveria retornar exit 8, recebeu $status"
[ ! -e "$UNSUPPORTED" ] ||
    fail "falha controlada de codegen deixou LLVM parcial"

echo "LLVM IR valida, controle de fluxo executavel e portavel nos casos do checkpoint."

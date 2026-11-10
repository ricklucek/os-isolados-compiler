#!/bin/sh
set -u

BIN=${1:-./macaronica}
LLVM_AS=${LLVM_AS:-llvm-as-18}
LLVM_OPT=${LLVM_OPT:-opt-18}
LLVM_LINK=${LLVM_LINK:-llvm-link-18}
MLIR_OPT=${MLIR_OPT:-mlir-opt-18}
MLIR_TRANSLATE=${MLIR_TRANSLATE:-mlir-translate-18}
CLANG_LLVM=${CLANG_LLVM:-clang-18}

WORK=.hybrid-test
OUT=.hybrid-test.out
EXPECTED=.hybrid-test.expected

cleanup() {
    rm -rf "$WORK" "$OUT" "$EXPECTED"
}
trap cleanup EXIT INT TERM
cleanup

fail() {
    echo "FALHA HYBRID: $1" >&2
    exit 1
}

for source in tests/hybrid/*.mcn; do
    base=$(basename "$source" .mcn)
    expected="tests/hybrid/$base.esperado"

    rm -rf "$WORK"
    LLVM_AS="$LLVM_AS" LLVM_OPT="$LLVM_OPT" LLVM_LINK="$LLVM_LINK"     MLIR_OPT="$MLIR_OPT" MLIR_TRANSLATE="$MLIR_TRANSLATE"     CLANG_LLVM="$CLANG_LLVM"         sh scripts/hybrid_pipeline.sh "$source" "$WORK" "$BIN" >/dev/null ||
        fail "pipeline hibrido falhou para $source"

    grep -q 'zl_mlir_kernel_l' "$WORK/00_scalar.ll" ||
        fail "modulo escalar nao chama kernel para $source"
    grep -q 'define .*@zl_mlir_kernel_l' "$WORK/kernels/04_kernel.ll" ||
        fail "modulo MLIR nao define kernel para $source"
    grep -q 'define .*@zl_mlir_kernel_l' "$WORK/05_linked.ll" ||
        fail "llvm-link nao resolveu kernel para $source"

    "$WORK/programa" > "$OUT" ||
        fail "executavel hibrido retornou erro para $source"
    tr -d '\r' < "$expected" > "$EXPECTED"
    cmp -s "$OUT" "$EXPECTED" ||
        fail "saida hibrida divergiu para $source"
done

echo "Integracao hibrida: LLVM escalar + kernels MLIR executam corretamente."

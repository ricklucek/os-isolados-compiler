#!/bin/sh
set -u

BIN=${1:-./macaronica}
MLIR_OPT=${MLIR_OPT:-mlir-opt-18}
MLIR_TRANSLATE=${MLIR_TRANSLATE:-mlir-translate-18}
LLVM_AS=${LLVM_AS:-llvm-as-18}

WORK=.mlir-test-work
INPUT=.mlir-test.mlir
LLVM=.mlir-test.ll

cleanup() {
    rm -rf "$WORK" "$INPUT" "$LLVM"
}
trap cleanup EXIT INT TERM
cleanup
mkdir -p "$WORK"

fail() {
    echo "FALHA MLIR: $1" >&2
    exit 1
}

for file in tests/dsl/validos/*.mcn; do
    "$BIN" "$file" --emit-mlir "$INPUT" >/dev/null ||
        fail "nao gerou MLIR para $file"
    "$MLIR_OPT" "$INPUT" -o /dev/null ||
        fail "mlir-opt rejeitou $file"

    rm -rf "$WORK"
    mkdir -p "$WORK"
    MLIR_OPT="$MLIR_OPT" MLIR_TRANSLATE="$MLIR_TRANSLATE"         sh scripts/pipeline.sh "$INPUT" "$WORK" >/dev/null ||
        fail "pipeline completo falhou para $file"
    "$LLVM_AS" "$WORK/04_kernel.ll" -o /dev/null ||
        fail "LLVM IR final de $file nao passa no llvm-as"
done

"$BIN" tests/dsl/validos/01_elementwise.mcn --emit-mlir "$INPUT" >/dev/null ||
    fail "nao gerou elementwise"
grep -q 'affine.for' "$INPUT" || fail "elementwise nao usa affine.for"
grep -q 'arith.addf' "$INPUT" || fail "promocao/adição flut ausente"
grep -q 'arith.sitofp' "$INPUT" || fail "promocao i32 -> f32 ausente"

MLIR_OPT="$MLIR_OPT" MLIR_TRANSLATE="$MLIR_TRANSLATE"     sh scripts/pipeline.sh "$INPUT" "$WORK" >/dev/null ||
    fail "pipeline elementwise falhou"

[ -s "$WORK/01_canonical.mlir" ] || fail "nivel canonical ausente"
[ -s "$WORK/02_scf.mlir" ] || fail "nivel SCF ausente"
[ -s "$WORK/03_llvm.mlir" ] || fail "nivel LLVM dialect ausente"
[ -s "$WORK/04_kernel.ll" ] || fail "LLVM IR final ausente"
grep -q 'scf.for' "$WORK/02_scf.mlir" ||
    fail "lower-affine nao produziu SCF"
grep -q 'llvm.func' "$WORK/03_llvm.mlir" ||
    fail "dialeto LLVM nao contem llvm.func"
"$LLVM_AS" "$WORK/04_kernel.ll" -o /dev/null ||
    fail "LLVM IR traduzida nao passa no llvm-as"

"$BIN" tests/dsl/validos/03_reducoes.mcn --emit-mlir "$INPUT" >/dev/null ||
    fail "nao gerou reducoes"
grep -q 'scf.for' "$INPUT" || fail "reducao nao usa scf.for"
grep -q 'iter_args' "$INPUT" || fail "reducao nao possui acumulador iter_args"

"$BIN" tests/dsl/validos/04_prodesc_promocao.mcn --emit-mlir "$INPUT" >/dev/null ||
    fail "nao gerou prodesc"
grep -q 'arith.mulf' "$INPUT" || fail "prodesc flut nao gera multiplicacao"
grep -q 'arith.addf' "$INPUT" || fail "prodesc flut nao gera reducao"
grep -q 'arith.sitofp' "$INPUT" || fail "prodesc nao promove inteira para flut"

echo "MLIR: kernels validos e pipeline completo ate LLVM IR."
